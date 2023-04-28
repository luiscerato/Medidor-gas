#include "lcd_ui.h"
#include "arduino.h"
#include <stdlib.h>
#include <time.h>

#define LOG_LOCAL_LEVEL ESP_LOGI
#include <esp_log.h>
static const char *TAG = "ui";

using namespace std;

void lcd_ui::ResetUpdateTime()
{
    UpdateTime = DftUpdateTime;
}

// lcd_ui::lcd_ui(int32_t w, int32_t h)
//{
//	lcd_ui(w, h, -1);
// }

lcd_ui::lcd_ui(int32_t w, int32_t h, int8_t SoundPin)
{
    ESP_LOGI(TAG);
    for (int32_t i = 0; i < UI_Max_Screens; i++)
        Screens[i].init();
    for (int32_t i = 0; i < UI_Max_Levels; i++)
        Screen_Stack[i] = 0;

    LCD.Widht = w;
    LCD.Height = h;
    ValAutoInc = AutoInc(1.0, 10.0);

    if (SoundPin > 0)
        Speaker.Init(SoundPin);

    ESP_LOGI(TAG, "LCD Widht: %d, Height: %d", LCD.Widht, LCD.Height);
}

lcd_ui::~lcd_ui()
{
    ESP_LOGI(TAG);
}

void lcd_ui::begin(LiquidCrystal *lcd, keyboard *keys)
{
    ESP_LOGI(TAG, "");
    this->lcd = lcd;
    this->keys = keys;

    // Agregar las ui del sistema
    Add_UI(ID_UI_Black, "@ui_black", &lcd_ui::UI_Black);
    Add_UI(ID_UI_SetVal, "@ui_set_val", &lcd_ui::UI_SetVal);
    Add_UI(ID_UI_SetString, "@ui_set_str", &lcd_ui::UI_SetStr);
    Add_UI(ID_UI_SetIP, "@ui_set_ip", &lcd_ui::UI_SetIP);

    Msg.begin(this, "ui_message", 189);

    OptionBox.begin(this, "ui_optionbox", 156);

    Question.begin(this, "ui_question", 0x1324);

    // DateTime.begin(this, "ui_datetime", 0x1000);

    Show(ID_UI_Black);

    SoundType = UI_Sound::None;
}

bool UI_Screen_t::Execute(lcd_ui *ui, UI_Action action)
{
    // if (function == nullptr || ui == nullptr)
    //	return false;
    if (ui_screen)
        return ui_screen->Run(ui, action);
    else if (ID < 0)
        return (ui->*member)(ui, action);
    else if (ID > 0)
        return function(ui, action);
    else
        return false;
};

void lcd_ui::Run()
{
    bool update = false;

    if (lcd == nullptr || keys == nullptr)
        return;

    keys->scan();

    // if (keys->isSomeKeyPressed() || Blinker.HasChanged())
    //	update = true;

    if (millis() - LastTime >= UpdateTime)
        update = true;

    if (update == false)
        return;

    LastTime = millis();

    int32_t screen = Screen_Stack[Screen_Index];
    if (!Screens[screen].isEmpty()) {
        InFunct = true;
        Screens[screen].Execute(this, UI_Action::Run);
        InFunct = false;
    }

    if (GoBack) {
        GoBack = false;
        Close(Result);
        SoundType = UI_Sound::Closing;
    }
    if (GoHome) {
        GoHome = false;
        Home();
        SoundType = UI_Sound::Closing;
    }

    if (SoundType != UI_Sound::None) {
        if (SoundType == UI_Sound::Beep)
            Speaker.PlayTone(2500, 50);
        else if (SoundType == UI_Sound::Enter)
            Speaker.PlayTone(3500, 100);
        else if (SoundType == UI_Sound::Closing)
            Speaker.PlayTone(1500, 100);
        SoundType = UI_Sound::None;
    }
    Speaker.Run();
}

bool lcd_ui::ShowByIndex(int32_t Index)
{
    ESP_LOGI(TAG, "UI: %i", Index);
    if (Index < 0 || Index >= UI_Max_Screens) {
        ESP_LOGI(TAG, "Error -> Index %i fuera de rango [%i..%i]", Index, 0, UI_Max_Screens);
        return false;
    }

    ResetUpdateTime();
    if (ClearScreenOnScreenSwitch)
        this->lcd->clear(); // Limpiar el lcd antes de mostrar la nueva pantalla
    this->keys->ClearKeys();

    Screen_Index++;
    Screen_Stack[Screen_Index] = Index;

    SoundType = UI_Sound::Enter;
    Screens[Index].Execute(this, UI_Action::Init);

    return true;
}

bool lcd_ui::Show(const char *UI)
{
    ESP_LOGI(TAG, "UI: %s", UI);
    int32_t Index;
    if (UI == nullptr) {
        ESP_LOGI(TAG, "Error -> nullpointer");
        return false;
    }
    if (Screen_Index == (UI_Max_Levels - 1)) {
        ESP_LOGI(TAG, "Error -> no hay espacio para mas pantallas [max: %i]", UI_Max_Levels);
        return false;
    }

    for (Index = 0; Index < UI_Max_Screens; Index++) {
        if (!Screens[Index].isEmpty()) {
            if (strcmp(UI, Screens[Index].Name) == 0) {
                return ShowByIndex(Index);
            }
        }
    }
    if (Index == UI_Max_Screens) {
        ESP_LOGI(TAG, "Error -> no se encuentra pantalla '%s'", UI);
        return false;
    }
    return false;
}

bool lcd_ui::Show(int32_t UI_ID)
{
    ESP_LOGI(TAG, "UI: 0x%0.8X", UI_ID);
    int32_t Index;

    if (Screen_Index == (UI_Max_Levels - 1)) {
        ESP_LOGI(TAG, "Error -> no hay espacio para mas pantallas [max: %i]", UI_Max_Levels);
        return false;
    }

    for (Index = 0; Index < UI_Max_Screens; Index++) {
        if (!Screens[Index].isEmpty()) {
            if (Screens[Index].ID == UI_ID)
                return ShowByIndex(Index);
        }
    }
    if (Index == UI_Max_Screens) {
        ESP_LOGI(TAG, "Error -> no se encuentra pantalla ID: %i", UI_ID);
        return false;
    }
    return false;
}

int32_t lcd_ui::GetScreenIndex()
{
    return Screen_Index;
}

bool lcd_ui::Close(UI_DialogResult result)
{
    bool res = false;
    ESP_LOGI(TAG, "Index: %i, result: %u", Screen_Index, result);
    Result = result; // Cargar el estado actual

    if (InFunct) { // Cuando se la llama desde una funci�n UI
        GoBack = true;
        if (Screen_Index)
            res = true;
    } else { // Cuando se la llama fuera de la funci�n UI
        GoBack = false;
        if (Screen_Index < 1)
            return false;

        SoundType = UI_Sound::Closing;
        int32_t screen = Screen_Stack[Screen_Index];
        Screen_Index--;
        int32_t lastIndex = Screen_Index; // Guardar el �ndice actual de la pantalla

        if (!Screens[screen].isEmpty()) { // LLamar a la pantalla que se est� cerrando
            res = Screens[screen].Execute(this, UI_Action::Closing);
        }
        this->keys->ClearKeys(); // Limpiar teclado
        if (ClearScreenOnScreenSwitch)
            this->lcd->clear(); // Limpiar el lcd antes de mostrar la nueva pantalla
        ResetUpdateTime();      // Tiempo de atualizaci�n por defecto

        if (lastIndex == Screen_Index) {         // Si el �ndice cambi� es porque se abri� una pantalla mientras se cerraba la actual
            screen = Screen_Stack[Screen_Index]; // Pedir a la anterior que recupere el entorno
            if (!Screens[screen].isEmpty()) {
                res = Screens[screen].Execute(this, UI_Action::Restore);
            }
        }
    }
    return res;
}

bool lcd_ui::setMainScreen(const char *UI)
{
    ESP_LOGI(TAG, "UI: %s", UI);
    int32_t Index;
    if (UI == nullptr) {
        ESP_LOGI(TAG, "Error -> nullpointer");
        return false;
    }

    for (Index = 0; Index < UI_Max_Screens; Index++) {
        if (!Screens[Index].isEmpty()) {
            if (strcmp(UI, Screens[Index].Name) == 0) {
                Screen_Stack[0] = Index;
                break;
            }
        }
    }
    if (Index == UI_Max_Screens) {
        ESP_LOGI(TAG, "Error -> no se encuentra pantalla '%s'", UI);
        return false;
    }
    Home();
    Screens[Index].Execute(this, UI_Action::Init);
    return true;
}

/*
        Borrar el stack de ventanas y mostrar la principal
*/
bool lcd_ui::Home()
{
    if (InFunct) {
        GoHome = true;
        return true;
    }
    while (Close(UI_DialogResult::Cancel))
        ;
    GoHome = false;
    return true;
}

void lcd_ui::SetUpdateTime(uint32_t Ms)
{
    UpdateTime = Ms;
}

Keys lcd_ui::GetKeys()
{
    static Keys LastKey = Keys::None;
    Keys key = keys->getNextKey();

    // if (key != Keys::None)
    //	Serial.printf("-Key: %d\n", key);

    if (!UsingKeyEnter && key == Keys::Esc)
        key = Keys::Enter;

    if (key != Keys::None) {
        switch (key) {
        case Keys::Left:
        case Keys::Right:
        case Keys::Esc:
        case Keys::Enter:
        case Keys::Ok:
        case Keys::Next:
            Blinker.Reset();
            break;
        default:
            Blinker.Set();
            break;
        }
        if (LastKey != key) {
            LastKey = key;
            SoundType = UI_Sound::Beep;
        }
    } else if (LastKey != Keys::None) {
        if (keys->isKeyUp(LastKey))
            LastKey = Keys::None;
    }
    return key;
}

void lcd_ui::PrintText(const char *Text, TextPos Pos, int32_t Widht)
{
    // ESP_LOGI(TAG, "LCD Widht: %d, Height: %d", LCD.Widht, LCD.Height);
    if (Widht < 1 || Widht > LCD.Widht)
        Widht = LCD.Widht;

    if (Pos == TextPos::Left) {
        lcd->printf("%-*.*s", Widht, Widht, Text);
        // ESP_LOGI(TAG, "PrintText: W=%d -> '%-*.*s'", Widht, Widht, Widht, Text);
    } else { // if (Pos == TextPos::Center) {
        int32_t len = strlen(Text), esp, count;
        if (len > Widht)
            len = Widht;
        esp = Widht - len;
        if (Pos == TextPos::Center)
            esp /= 2;
        count = esp;
        while (count-- > 0)
            lcd->write(' ');

        count = Widht - esp;
        lcd->printf("%.*s", count, Text);
        count -= len;
        while (count-- > 0)
            lcd->write(' ');
    }
}

int32_t lcd_ui::Add_UI(int32_t ID, const char *Name, bool (lcd_ui::*member)(lcd_ui *, UI_Action))
{
    int32_t Index;
    if (member == nullptr || Name == nullptr || ID >= 0) {
        ESP_LOGI(TAG, "Error -> nullpointer or ID > 0");
        return -2;
    }
    ESP_LOGI(TAG, "Sys ui name: %s, ID:0x%0.8X at 0x%0.8X", Name, ID, member);

    for (Index = 0; Index < UI_Max_Screens; Index++) {
        if (Screens[Index].isEmpty())
            break;
        else {
            if (strcmp(Name, Screens[Index].Name) == 0) {
                ESP_LOGI(TAG, "Error -> pantalla '%s' ya existe", Name);
                return -3;
            } else if (ID == Screens[Index].ID) {
                ESP_LOGI(TAG, "Error -> pantalla ID=%i ya existe", ID);
                return -4;
            }
        }
    }
    if (Index == UI_Max_Screens) {
        ESP_LOGI(TAG, "Error -> no se pueden agregar mas pantallas [max=%i]", UI_Max_Screens);
        return -1;
    }
    Screens[Index].Name = Name;
    Screens[Index].ID = ID;
    Screens[Index].member = member;
    return 0;
}

int32_t lcd_ui::Add_UI(int32_t ID, const char *Name, bool (*func)(lcd_ui *, UI_Action))
{
    int32_t Index;
    if (func == nullptr || Name == nullptr || ID < 0) {
        ESP_LOGI(TAG, "Error -> nullpointer or ID < 0");
        return -2;
    }
    ESP_LOGI(TAG, "Name: %s, ID:0x%0.8X at 0x%0.8X", Name, ID, func);

    for (Index = 0; Index < UI_Max_Screens; Index++) {
        if (Screens[Index].isEmpty())
            break;
        else {
            if (strcmp(Name, Screens[Index].Name) == 0) {
                ESP_LOGI(TAG, "Error -> pantalla '%s' ya existe", Name);
                return -3;
            } else if (ID == Screens[Index].ID) {
                ESP_LOGI(TAG, "Error -> pantalla ID=%i ya existe", ID);
                return -4;
            }
        }
    }
    if (Index == UI_Max_Screens) {
        ESP_LOGI(TAG, "Error -> no se pueden agregar mas pantallas [max=%i]", UI_Max_Screens);
        return -1;
    }
    Screens[Index].Name = Name;
    Screens[Index].ID = ID;
    Screens[Index].function = func;
    return 0;
}

int32_t lcd_ui::Add_UI(int32_t ID, const char *Name, lcd_ui_screen &ui_screen)
{
    int32_t Index;
    if (Name == nullptr || ID <= 0) {
        ESP_LOGI(TAG, "Error -> nullpointer or ID <= 0");
        return -2;
    }

    ESP_LOGI(TAG, "lcd_ui_screen: %s, ID:0x%0.8X at 0x%0.8X", Name, ID, ui_screen);

    for (Index = 0; Index < UI_Max_Screens; Index++) {
        if (Screens[Index].isEmpty())
            break;
        else {
            if (strcmp(Name, Screens[Index].Name) == 0) {
                ESP_LOGI(TAG, "Error -> pantalla '%s' ya existe", Name);
                return -3;
            } else if (ID == Screens[Index].ID) {
                ESP_LOGI(TAG, "Error -> pantalla ID=%i ya existe", ID);
                return -4;
            }
        }
    }
    if (Index == UI_Max_Screens) {
        ESP_LOGI(TAG, "Error -> no se pueden agregar mas pantallas [max=%i]", UI_Max_Screens);
        return -1;
    }
    Screens[Index].Name = Name;
    Screens[Index].ID = ID;
    Screens[Index].ui_screen = &ui_screen;
    return 0;
}

int32_t lcd_ui::Remove_UI(const char *Name)
{
    if (Name == nullptr) {
        ESP_LOGI(TAG, "Error -> nullpointer");
        return -2;
    }

    for (int32_t Index = 0; Index < UI_Max_Screens; Index++) {
        if (!Screens[Index].isEmpty()) {
            if (strcmp(Name, Screens[Index].Name) == 0) {
                Screens[Index].init();
                return 0;
            }
        }
    }
    ESP_LOGI(TAG, "Error -> no se encuentra pantalla '%s'", Name);
    return -1;
}

int32_t lcd_ui::Remove_UI(int32_t ID)
{
    if (ID == 0)
        return -2;

    for (int32_t Index = 0; Index < UI_Max_Screens; Index++) {
        if (!Screens[Index].isEmpty()) {
            if (ID == Screens[Index].ID) {
                Screens[Index].init();
                return 0;
            }
        }
    }
    ESP_LOGI(TAG, "Error -> no se encuentra pantalla ID='%i'", ID);
    return -1;
}

void lcd_ui::PrintState()
{
    Serial.printf("UI screen index: %u\n", Screen_Index);
    Serial.printf("Screen stack:\n", Screen_Index);
    for (int32_t i = Screen_Index; i >= 0; i--) {
        Serial.printf("Pos: %.2X, ID:0x%0.4X, Func:0x%0.8X, Name: %s\n",
                      i, Screens[Screen_Stack[i]].ID, Screens[Screen_Stack[i]].function, Screens[Screen_Stack[i]].Name);
    }
}

void lcd_ui::PrintScreenList()
{
    Serial.printf("Lista de pantallas caragadas en UI\n", Screen_Index);
    int i;
    for (i = 0; i < UI_Max_Screens; i++) {
        if (!Screens[i].isEmpty()) {
            Serial.printf(" Index: %u -> [%s] ID:%p '%s'\n", i, Screens[i].ToString(), Screens[i].ID, Screens[i].Name);
        }
    }
    Serial.printf("Se encontraron %i pantallas\n", Screen_Index);

    if (i > 0)
        Serial.printf("La pantalla principal es '%s'\n", Screens[Screen_Stack[0]].Name);
}

/*
        Funciones de sistema
*/

/*
        Versiones INT
*/
bool lcd_ui::Show_SetVal(const char *Title, int32_t *Val, int32_t Min, int32_t Max, ValFormat_t &Format)
{
    this->Title = Title;
    this->Val.Int.setParams(Val, Min, Max);
    this->Format = Format;
    this->ValAutoInc.setInitEnd(1, 1000);
    this->ValAutoInc.Print();

    return Show(ID_UI_SetVal);
}

bool lcd_ui::Show_SetVal(const char *Title, int32_t *Val, int32_t Min, int32_t Max)
{
    Format.setDefault();
    return Show_SetVal(Title, Val, Min, Max, Format);
}

bool lcd_ui::Show_SetVal(const char *Title, int32_t *Val)
{
    Format.setDefault();
    return Show_SetVal(Title, Val, 0, 0, Format);
}

/*
        Versiones UNSIGNED INT
*/
bool lcd_ui::Show_SetVal(const char *Title, uint32_t *Val, uint32_t Min, uint32_t Max, ValFormat_t &Format)
{
    this->Title = Title;
    this->Val.UInt.setParams(Val, Min, Max);
    this->Format = Format;
    this->ValAutoInc.setInitEnd(1, 1000);
    this->ValAutoInc.Print();

    return Show(ID_UI_SetVal);
}

bool lcd_ui::Show_SetVal(const char *Title, uint32_t *Val, uint32_t Min, uint32_t Max)
{
    Format.setDefault();
    return Show_SetVal(Title, Val, Min, Max, Format);
}

bool lcd_ui::Show_SetVal(const char *Title, uint32_t *Val)
{
    Format.setDefault();
    return Show_SetVal(Title, Val, 0, 0, Format);
}

/*
        Versiones Float
*/
bool lcd_ui::Show_SetVal(const char *Title, float *Val, float Min, float Max, ValFormat_t &Format)
{
    this->Title = Title;
    this->Val.Float.setParams(Val, Min, Max);
    this->Format = Format;
    float Inc = 0.0001;
    float End = 0.1;
    if (Format.DecimalPoint > 0) {
        Inc = 1.0 / powf(10.0, Format.DecimalPoint);
        End = Inc * 1000.0;
    }
    this->ValAutoInc.setInitEnd(Inc, End);

    this->ValAutoInc.Print();
    return Show(ID_UI_SetVal);
}

bool lcd_ui::Show_SetVal(const char *Title, float *Val, float Min, float Max)
{
    Format.setDefault();
    return Show_SetVal(Title, Val, Min, Max, Format);
}

bool lcd_ui::Show_SetVal(const char *Title, float *Val)
{
    Format.setDefault();
    return Show_SetVal(Title, Val, 0.0, 0.0, Format);
}

bool lcd_ui::Show_SetIPAddress(const char *Title, uint32_t *IPs, int32_t Count)
{
    if (Count > 5)
        return false;
    else if (Count < 1)
        return false;

    this->Title = Title;
    this->IPs = new IPAddress[Count];
    if (this->IPs == nullptr)
        return false;
    for (int32_t i = 0; i < Count; i++) {
        this->IPs[i] = IPs[i];
    }
    this->IPCount = Count;
    this->IPsrc = nullptr;
    this->IPIntsrc = IPs;

    Show(ID_UI_SetIP);
    return true;
}

bool lcd_ui::Show_SetIPAddress(const char *Title, IPAddress *IPs, int32_t Count)
{
    if (Count > 5)
        return false;
    else if (Count < 1)
        return false;

    this->Title = Title;
    this->IPs = new IPAddress[Count];
    if (this->IPs == nullptr)
        return false;
    for (int32_t i = 0; i < Count; i++) {
        this->IPs[i] = IPs[i];
    }
    this->IPCount = Count;
    this->IPsrc = IPs;
    this->IPIntsrc = nullptr;

    Show(ID_UI_SetIP);
    return true;
}

/*
        Strings
*/
bool lcd_ui::Show_SetString(const char *Title, char *Src, uint32_t MaxLenght)
{
    this->StrVal.Str.clear();
    this->Title = Title;
    this->StrVal.Src = Src;
    this->StrVal.Str = Src;
    this->StrVal.MaxCar = MaxLenght;

    Show(ID_UI_SetString);

    return true;
}

bool lcd_ui::UI_Black(lcd_ui *ui, UI_Action action)
{
    if (action == UI_Action::Init) {
        ESP_LOGI(TAG, "UI_Action::Init");
        lcd->clear();
        return true;
    } else if (action == UI_Action::Closing) {
        ESP_LOGI(TAG, "UI_Action::Closing");
        return true;
    } else if (action == UI_Action::Run) {
        return true;
    } else if (action == UI_Action::Restore) {
        ESP_LOGI(TAG, "UI_Action::Restore");
        lcd->clear();
        return true;
    }
    ESP_LOGI(TAG, "UI_Action::Unknown -> %i", action);
    return false;
}

bool lcd_ui::UI_SetVal(lcd_ui *ui, UI_Action action)
{
    static ValToString Conv(ui->getWidht());
    static uint64_t LastTime;
    char Result[32];

    if (action == UI_Action::Init) {
        Conv.init(ui->getWidht());
        ValAutoInc.setKeyboard(this->keys);
        ui->lcd->clear();
        return true;
    } else if (action != UI_Action::Run)
        return false;

    // Serial.printf("ui -> %lu\n", millis());

    ui->lcd->setCursor(0, 0);
    ui->lcd->printf("%.*s", ui->getWidht(), Title.c_str());

    Conv.setFormat(&Format);

    ui->lcd->setCursor(0, 1);
    if (Val.getType() == ValType::Integer)
        Conv.Convert(Val.Int.getVal(), Result, ui->Blinker.Update());
    else if (Val.getType() == ValType::UInteger)
        Conv.Convert(Val.UInt.getVal(), Result, ui->Blinker.Update());
    else if (Val.getType() == ValType::Float)
        Conv.Convert(Val.Float.getVal(), Result, ui->Blinker.Update());
    else
        strcpy(Result, "Error type");

    ui->lcd->printf("%s", Result);

    if (ui->getHeight() > 2) {
        if (Val.Int.getMin() != Val.Int.getMax()) {
            ui->lcd->setCursor(0, 3);
            ui->lcd->printf("%d..%d", Val.Int.getMin(), Val.Int.getMax());
        }
    }

    Keys key = ui->GetKeys();

    ValAutoInc.UpdateKey(key);
    Val.SetInc(ValAutoInc.getInc());

    switch (key) {
    case Keys::Enter:
        Val.Save();
        ui->Close(UI_DialogResult::Ok);
        break;

    case Keys::Esc:
        ui->Close(UI_DialogResult::Cancel);
        break;

    case Keys::Up:
        Val.AddInc();
        break;

    case Keys::Down:
        Val.SubInc();
        break;
    }
    return true;
}

bool lcd_ui::UI_SetStr(lcd_ui *ui, UI_Action action)
{
    static int32_t Pos = 0, Offset = 0;
    static bool Save = false;

    if (action == UI_Action::Init) {
        Pos = 0;
        Save = true;
        return true;
    } else if (action != UI_Action::Run)
        return false;

    ui->lcd->setCursor(0, 0);
    ui->lcd->printf("%-20.20s", ui->Title.c_str());

    int32_t str_len, str_end = ui->StrVal.Str.lastIndexOf((char)0x1F); // Buscar caracter de terminacion de string
    str_len = ui->StrVal.Str.length();
    if (str_end < 0)
        str_end = str_len;

    if (ui->StrVal.MaxCar > 0) { // Limitar la cantidad de caracteres.
        if (str_end > ui->StrVal.MaxCar - 1)
            str_end = ui->StrVal.MaxCar - 1;
    }

    ui->lcd->setCursor(0, 1);
    for (int32_t i = 0; i < ui->getWidht(); i++) {
        if (!ui->Blinker.Update() && (Pos - Offset) == i) {
            if (ui->StrVal.Str[i + Offset] == ' ')
                ui->lcd->print('_');
            else
                ui->lcd->print(' ');
        } else {
            if (i + Offset < str_end) {
                ui->lcd->print(ui->StrVal.Str[i + Offset]);
            } else if ((i + Offset == str_end) && (Pos - Offset == i)) {
                ui->lcd->print((char)0xFF);
            } else {
                ui->lcd->print(' ');
            }
        }
    }

    ui->lcd->setCursor(0, 2);
    ui->lcd->printf("Largo: %-*u", ui->getWidht() - 7, str_end);

    ui->lcd->setCursor(0, 3);
    if (Pos > str_end && !ui->Blinker.Update())
        ui->lcd->printf("%*s", ui->getWidht(), " ");
    else
        ui->lcd->printf("%s", Save ? "      Guardar       " : "       Volver       ");

    char c;
    int32_t Inc = 0;
    switch (ui->GetKeys()) {
    case Keys::Left:
        Inc = -1;

    case Keys::Next:
        if (Inc == 0)
            Inc = 1;

        Pos += Inc;
        if (Pos == str_end) // Encender el caracter de fin de string
            ui->Blinker.Set();

        if (Pos > str_end + 1)
            Pos = 0;
        else if (Pos < 0)
            Pos = str_end + 1;

        Inc = Pos < str_end ? Pos : str_end;
        if (Inc < Offset)
            Offset = Inc;
        else if (Inc > (Offset + (ui->getWidht() - 1)))
            Offset = Inc - (ui->getWidht() - 1);

        Serial.printf("Enter: Pos=%i, len=%i\n", Pos, str_end);
        break;

    case Keys::Enter:
        Save = true;
    case Keys::Esc:
        if (Save) {
            ui->StrVal.Str[str_end] = 0;
            if (ui->StrVal.HasChanged())
                ui->StrVal.Save();
            ui->Close(UI_DialogResult::Ok);
            break;
        }
        ui->Close(UI_DialogResult::Cancel);
        break;

    case Keys::Right:
        if (Pos > str_end)
            break;
        c = ui->StrVal.Str[Pos];
        if (c >= ' ' && c < '0')
            c = '0';
        else if (c >= '0' && c <= '9')
            c = ':';
        else if (c > '9' && c < 'A')
            c = 'A';
        else if (c >= 'A' && c <= 'Z')
            c = '[';
        else if (c > 'Z' && c < 'a')
            c = '{';
        else if (c > '}')
            c = 0x19;
        else if (c == 0x19)
            c = ' ';
        else
            c = ' ';
        ui->StrVal.Str[Pos] = c;
        break;

    case Keys::Down:
        Inc = -1;
    case Keys::Up:
        if (Inc == 0)
            Inc = 1;

        if (Pos > str_end) {
            Save = !Save;
        } else if (Pos == ui->StrVal.Str.length()) {
            if (ui->StrVal.Str.length() == 0)
                c = '0';
            else {
                c = ui->StrVal.Str.charAt(str_len - 1);
                c += Inc;
            }
            ui->StrVal.Str.concat(c);
        } else {
            ui->StrVal.Str[Pos] += Inc;
            if (ui->StrVal.Str[Pos] < 0x1F)
                ui->StrVal.Str[Pos] = '}';
            else if (ui->StrVal.Str[Pos] > '}')
                ui->StrVal.Str[Pos] = 0x1F;
        }
        Serial.printf("Down: Pos=%i, len=%i, end=%i, car=%c -> 0x%.2X\n", Pos, ui->StrVal.Str.length(), str_end, ui->StrVal.Str[Pos], ui->StrVal.Str[Pos]);
        break;
    }
    return true;
}

bool lcd_ui::UI_SetIP(lcd_ui *ui, UI_Action action)
{
    static const char *IPNames[] = {"IP:", "Mask:", "GW:", "DNS1:", "DNS2:"};
    static OffsetIndex Index;
    static int32_t Pos = 0;
    char Conv[16];
    static bool Save = true;

    if (action == UI_Action::Init) {
        ValAutoInc.setKeyboard(this->keys);
        ValAutoInc.setInitEnd(1, 10);
        Index.setParams(0, ui->IPCount, ui->getHeight() - 1);
        Index.Print();
        return true;
    } else if (action == UI_Action::Closing) {
        delete[] ui->IPs;
        ui->IPsrc = nullptr;
        ui->IPIntsrc = nullptr;
        ui->IPCount = 0;
    } else if (action != UI_Action::Run)
        return false;

    ui->lcd->setCursor(0, 0);
    ui->lcd->printf("%-*.*s %u/%u", ui->getWidht() - 4, ui->getWidht() - 4, Title.c_str(), (int)Index + 1, Index.getMax() + 1);

    for (int x = 0; x < Index.getItemWindow(); x++) {
        int i = Index.getOffset() + x;
        sprintf(Conv, "%u.%u.%u.%u", ui->IPs[i][0], ui->IPs[i][1], ui->IPs[i][2], ui->IPs[i][3]);

        if (i == (Pos / 4) && !ui->Blinker.Update()) {
            char *p = Conv;
            int z = Pos % 4;
            while (z > 0) {
                if (*p == '.')
                    z--;
                else if (*p == 0)
                    break;
                p++;
            }
            while (*p >= '0' && *p <= '9')
                *p++ = ' ';
        }

        ui->lcd->setCursor(0, 1 + x);
        if (i < Index.getMax())
            ui->lcd->printf("%-5.5s%15s", IPNames[i], Conv);
        else if (i == Index.getMax() && ui->Blinker.Update())
            ui->PrintText(Save ? "Guardar" : "Volver", TextPos::Right);
        // ui->lcd->printf("%-*.*s", ui->getWidht(), ui->getWidht(), Save ? "       Guardar      ": "        Volver      ");
        else
            ui->lcd->printf("%*.*s", ui->getWidht(), ui->getWidht(), " ");
    }

    Keys key = ui->GetKeys();
    ValAutoInc.UpdateKey(key);
    bool Dec = false;
    int x = Pos / 4, y = Pos % 4, z;

    union Z
    {
        uint32_t Val;
        uint8_t Array[4];
    } ip;
    ip.Val = ui->IPs[x];

    switch (key) {
    case Keys::Next:
        Pos++;
        if (Pos > ui->IPCount * 4)
            Pos = 0;
        Index = Pos / 4;
        break;

    case Keys::Enter:
        Save = true;
    case Keys::Esc:
        if (Save && (ui->IPsrc || IPIntsrc)) {
            for (y = 0; y < ui->IPCount; y++) {
                if (IPsrc)
                    ui->IPsrc[y] = ui->IPs[y];
                else
                    IPIntsrc[y] = ui->IPs[y];
            }

            ui->Close(UI_DialogResult::Ok);
            break;
        }
        ui->Close(UI_DialogResult::Cancel);
        break;

    case Keys::Down:
        Dec = true;
    case Keys::Up:
        if (Pos < ui->IPCount * 4) {
            z = ip.Array[y];
            if (Dec == true)
                z -= ValAutoInc.getInc();
            else
                z += ValAutoInc.getInc();

            if (z < 0)
                z = 0;
            else if (z > 255)
                z = 255;
            ip.Array[y] = z;

            ui->IPs[x] = ip.Val;
        } else {
            Save = !Save;
        }

        break;
    }
    return true;
}

/*
                ***********
                Class ValToString
*/

ValToString::ValToString(int32_t Widht)
{
    init(Widht);
}

void ValToString::init(int32_t Widht)
{
    Type = ValType::Undefined;
    Result[0] = 0;
    Format.setDefault();

    if (Widht < 0 || Widht > 32)
        return;
    StrWidht = Widht;
}

void ValToString::setFormat(ValFormat_t *fmt)
{
    Format = *fmt;

    if (Format.DecimalPoint < 0)
        Format.DecimalPoint = 0;
    else if (Format.DecimalPoint > 9)
        Format.DecimalPoint = 9;

    if (Format.Width < 0)
        Format.Width = 0;
    else if (Format.Width > 19)
        Format.Width = 19;

    if (Format.Prefix && Format.Base == ValBase::Decimal)
        Format.Prefix = false;

    if (Format.DecimalPoint > 0 && Format.Base != ValBase::Decimal)
        Format.DecimalPoint = 0;
}

// Float
int32_t ValToString::Convert(float Val)
{
    this->Val.Float = Val;
    Type = ValType::Float;

    return Convert();
}

int32_t ValToString::Convert(float Val, char *dest, bool Blink)
{
    this->Val.Float = Val;
    Type = ValType::Float;

    return FormatDest(dest, Blink);
}

// Int
int32_t ValToString::Convert(int32_t Val)
{
    this->Val.Int = Val;
    Type = ValType::Integer;

    return Convert();
}

int32_t ValToString::Convert(int32_t Val, char *dest, bool Blink)
{
    this->Val.Int = Val;
    Type = ValType::Integer;

    return FormatDest(dest, Blink);
}

// Uint
int32_t ValToString::Convert(uint32_t Val)
{
    this->Val.UInt = Val;
    Type = ValType::UInteger;

    return Convert();
}

int32_t ValToString::Convert(uint32_t Val, char *dest, bool Blink)
{
    this->Val.UInt = Val;
    Type = ValType::UInteger;

    return FormatDest(dest, Blink);
}

int32_t ValToString::FormatDest(char *dest, bool Blink)
{
    int32_t unit = strlen(Format.Unit);
    int32_t widht = Convert();
    int32_t i = (StrWidht - widht - unit) / 2;

    widht = snprintf(dest, StrWidht, "%*s%*s%s%*s", // Formato de conversi�n: <espacios><valor><unidad><espacios>
                     i, " ",                        // Espacios en blanco a la derecha
                     widht, Blink ? Result : " ",   // Valor convertido, o bloque de espacios
                     Format.Unit,                   // Unidad
                     i + 1, " ");                   // Espacios a la izquierda

    return widht;
}

int32_t ValToString::Convert()
{
    char fmt[16], *p, prefix;
    p = fmt;

    if (Type == ValType::Undefined)
        return 0;

    if (Format.Base == ValBase::Octal)
        prefix = 'o';
    else if (Format.Base == ValBase::Hexadecimal)
        prefix = 'x';
    else
        prefix = 0;

    /*
            Crear el string de formato de visualizacion
            %[flags][width][.precision][length]specifier
    */
    *p++ = '%'; // Marca de inicio de formato
    if (Format.PositiveSign)
        *p++ = '+'; // Flag Forces to preceed the result with a plus or minus sign (+ or -) even for positive numbers.
    if (Format.Prefix)
        *p++ = '#'; // Flag Used with o, x or X specifiers the value is preceeded with 0, 0x or 0X respectively for values different than zero.
    if (Format.FillZeros)
        *p++ = '0'; // Flag Left-pads the number with zeroes (0)

    int32_t widht = Format.Width; // Minimum number of characters to be printed.
    if (widht > 0) {
        if (Type != ValType::Float && Format.DecimalPoint > 0 && Format.Base == ValBase::Decimal) {
            widht -= Format.DecimalPoint + 1; // Ajustar ancho por punto decimal en enteros (ver m�s abajo)
        }
        if (widht > 9)
            *p++ = (widht / 10) + '0';
        if (widht > 0)
            *p++ = (widht % 10) + '0';
    }

    //[.precision] para los distintos tipos
    if (Type == ValType::Float) { // Float
        if (Format.Base == ValBase::Decimal) {
            if (Format.DecimalPoint > 0) {
                *p++ = '.';
                if (Format.DecimalPoint > 9)
                    *p++ = (Format.DecimalPoint / 10) + '0';
                *p++ = (Format.DecimalPoint % 10) + '0';
            }
            *p++ = 'f';
        } else {
            *p++ = prefix;
        }
        *p = 0;
        widht = sprintf(Result, fmt, Val.Float);
    } else {                                                              // Enteros
        if (Format.DecimalPoint > 0 && Format.Base == ValBase::Decimal) { // Con punto decimal
            // Para facilitar la muestra de enteros con puntos decimales se lo va a dividir
            //  en dos partes, la entera y la decimal y a mostrar en dos partes con dos variables distintas
            *p++ = 'd';
            *p++ = '.'; // Formato de la parte 'decimal'
            *p++ = '%';
            *p++ = '0';                  // Rellenar con 0
            if (Format.DecimalPoint > 9) // ancho del campo
                *p++ = (Format.DecimalPoint / 10) + '0';
            *p++ = (Format.DecimalPoint % 10) + '0';
            *p++ = 'd'; // parte 'decimal' como entero con signo
            *p = 0;

            // Calcula la parte entera y la parte decimal
            int32_t intp, decp;
            bool adds = false;
            if (Type == ValType::UInteger) {
                intp = (int64_t)Val.UInt / Divider[Format.DecimalPoint]; // Parte entera
                decp = (int64_t)Val.UInt % Divider[Format.DecimalPoint]; // Parte decimal
            } else {
                intp = Val.Int / Divider[Format.DecimalPoint]; // Parte entera
                decp = Val.Int % Divider[Format.DecimalPoint]; // Parte decimal
                if (decp < 0) {
                    decp = -decp;
                    if (intp == 0) { // Si la parte entera es == 0 y la parte decimal es negativa, poner intp como -1 para forzar a poner el signo negativo
                        adds = true; // Marcar que se hizo ese forzado
                        intp = -1;
                    }
                }
            }

            widht = sprintf(Result, fmt, intp, decp);
            if (adds) { // En el caso de haber forzado el resultado, buscar el -1 y cambiarlo por un -0
                char *trick = strstr(Result, "-1");
                if (trick)
                    *++trick = '0';
            }
        } else { // Sin punto decimal
            if (Type == ValType::Integer) {
                if (Format.Base != ValBase::Decimal)
                    *p++ = prefix;
                else
                    *p++ = 'd';
                *p = 0;
                widht = sprintf(Result, fmt, Val.Int);
            } else { // Unsigned
                if (Format.Base != ValBase::Decimal)
                    *p++ = prefix;
                else
                    *p++ = 'u';
                *p = 0;
                widht = sprintf(Result, fmt, Val.UInt);
            }
        }
    }
    return widht;
}

/*
        Constructores de la clase
*/
lcd_ui_screen::lcd_ui_screen()
{
    ESP_LOGI(TAG, "");
    // ui = nullptr;
    MyID = 20000;
}

lcd_ui_screen::~lcd_ui_screen()
{
    ESP_LOGI(TAG, "");
}

/**/
bool lcd_ui_screen::Run(lcd_ui *ui, UI_Action action)
{
    return true;
}

bool lcd_ui_screen::Show()
{
    if (ui == nullptr)
        return false;
    return ui->Show(MyID);
}

bool lcd_ui_screen::Close(UI_DialogResult Result)
{
    ESP_LOGI(TAG, "Result: %u", Result);
    if (ui == nullptr)
        return false;
    return ui->Close(Result);
}

bool lcd_ui_screen::begin(lcd_ui *main_ui, const char *Name, int32_t ID)
{
    ESP_LOGI(TAG, "%p", main_ui);
    ui = main_ui;
    MyID = ID;
    if (ui == nullptr) {
        ESP_LOGI(TAG, "No se puede iniciar con ui==null");
        return false;
    }

    ui->Add_UI(MyID, Name, *this);
    return true;
}

bool Screen_Message::Run(lcd_ui *ui, UI_Action action)
{
    if (action == UI_Action::Init || action == UI_Action::Restore) {
        ESP_LOGI(TAG, "action = %u", action);
        LastTime = millis();
        return true;
    } else if (action == UI_Action::Closing) {
        ESP_LOGI(TAG, "action = %u", action);
        Title.clear();
        Message.clear();
        return true;
    } else if (action != UI_Action::Run) {
        ESP_LOGI(TAG, "action = %u", action);
        return false;
    }

    ui->lcd->setCursor(0, 0);
    ui->PrintText(Title.c_str(), TextPos::Left);

    ui->lcd->setCursor(0, 1);
    ui->PrintText(" ", TextPos::Center);
    ui->lcd->setCursor(0, 2);
    ui->PrintText(Message.c_str(), TextPos::Center);
    ui->lcd->setCursor(0, 3);
    ui->PrintText(" ", TextPos::Center);

    if ((Timeout) && (millis() - LastTime > Timeout)) {
        ESP_LOGI(TAG, "timeout! time= %u ms of %u ms", millis() - LastTime, Timeout);
        Close(UI_DialogResult::Ok);
    }

    Keys key = ui->GetKeys();

    if (key != Keys::None) {
        ESP_LOGI(TAG, "cerrando por tecla -> %u", key);
        Close(UI_DialogResult::Ok);
    }

    return true;
}

bool Screen_Message::ShowMessage(char *Title, char *Msg)
{
    ESP_LOGI(TAG, "Title = %s, Msg = %s", Title, Msg);

    this->Title = Title;
    this->Message = Msg;
    this->Timeout = 2000;
    this->LastTime = millis();

    return Show();
}

bool Screen_Message::ShowMessage(char *Title, char *Msg, uint32_t Timeout)
{
    ESP_LOGI(TAG, "Title = %s, Msg = %s, timeout = %u", Title, Msg, Timeout);
    this->Title = Title;
    this->Message = Msg;
    if (Timeout == 0)
        this->Timeout = 0;
    else {
        this->Timeout = constrain(Timeout, 1000, 20000);
    }
    this->LastTime = millis();

    return Show();
}

bool Screen_Option::Run(lcd_ui *ui, UI_Action action)
{
    static int32_t Index;
    if (action == UI_Action::Init || action == UI_Action::Restore) {
        ESP_LOGI(TAG, "action = %u", action);
        ui->lcd->clear();
        if (List == nullptr)
            return false;

        Index = -1;
        Serial.printf("Options -> dir= %p. myval=%p. val: %u\n", this->List, MyVal, *MyVal);
        for (int x = 0; x < Count; x++) {
            Serial.printf("x= %u -> dir= %p\n", x, (List + x));
            // if ((List + x)->Val == *MyVal) {
            if (List[x].Val == *MyVal) {
                Index = x;
                ESP_LOGI(TAG, "Index = %u -> %s", Index, (List + x)->Name);
                break;
            }
        }

        ESP_LOGI(TAG, "MyVal = %i -> Index = %i", *MyVal, Index);
        return true;
    } else if (action == UI_Action::Closing) {
        ESP_LOGI(TAG, "action = %u", action);
        Title.clear();
        if (ToErase)
            delete[] List;
        ToErase = false;
        return true;
    } else if (action != UI_Action::Run) {
        ESP_LOGI(TAG, "action = %u", action);
        return false;
    }

    ui->lcd->setCursor(0, 0);
    ui->PrintText(Title.c_str(), TextPos::Left);

    char *opt, *inf;
    if (Index < 0) {
        opt = "Opcion no disponible";
        inf = "Error de seleccion";
    } else {
        opt = (List + Index)->Name;
        inf = (List + Index)->Info;
    }

    ui->lcd->setCursor(0, ui->getHeight() / 2);
    if (ui->Blinker.Update())
        ui->PrintText(opt, TextPos::Center);
    else
        ui->PrintText("", TextPos::Center);

    if (ui->getHeight() > 2 && inf) {
        ui->lcd->setCursor(0, ui->getHeight() - 1);
        ui->PrintText(inf, TextPos::Left);
    }

    Keys key = ui->GetKeys();
    switch (key) {
    case Keys::Down:
        Index--;
        if (Index < 0)
            Index = 0;
        break;

    case Keys::Up:
        Index++;
        if (Index >= Count)
            Index = Count - 1;
        break;

    case Keys::Enter:
        if (Index >= 0 && Index < Count && MyVal) {
            *MyVal = (List + Index)->Val;
            ESP_LOGI(TAG, "Save! new val = %i", *MyVal);
        }
        Close(UI_DialogResult::Ok);
        break;

    case Keys::Esc:
        ESP_LOGI(TAG, "No se guardan datos");
        Close(UI_DialogResult::Cancel);
        break;
    }
    return true;
}

bool Screen_Option::ShowList(char *Title, int32_t *myVal, const Option *Options, uint32_t Count)
{
    ESP_LOGI(TAG, "");
    if (Options == nullptr || myVal == nullptr || Count < 1)
        return false;

    this->Title = Title;
    this->MyVal = myVal;
    this->List = (Option *)Options;
    this->Count = Count;
    ToErase = false;
    Serial.printf("Options -> dir= %p. MyVal: %d\n", this->List, *myVal);

    return Show();
}

bool Screen_Option::ShowList(char *Title, int32_t *myVal, int32_t *Val, char **Names, char **Info, uint32_t Count)
{
    ESP_LOGI(TAG, "");
    if (Val == nullptr || Names == nullptr || myVal == nullptr || Count < 1)
        return false;
    this->Title = Title;
    this->MyVal = myVal;
    this->Count = Count;
    ToErase = true;

    List = new Option[Count]; // Crear un array en memoria con los punteros a las opciones
    if (List == nullptr) {
        ESP_LOGI(TAG, "No se pudo crear lista de %u elementos.", Count);
        return false;
    }

    for (int i = 0; i < Count; i++) {
        (List + i)->Name = Names[i];
        (List + i)->Info = Info ? Info[i] : nullptr;
        (List + i)->Val = Val[i];
    }
    return Show();
}

bool Screen_Question::Run(lcd_ui *ui, UI_Action action)
{
    if (action == UI_Action::Init || action == UI_Action::Restore) {
        ESP_LOGI(TAG, "action = %u", action);
        ui->lcd->clear();
        if (action == UI_Action::Init)
            Index = 0;
        return true;
    } else if (action == UI_Action::Closing) {
        ESP_LOGI(TAG, "action = %u", action);
        return true;
    } else if (action != UI_Action::Run) {
        ESP_LOGI(TAG, "action = %u", action);
        return false;
    }

    ui->lcd->setCursor(0, 0);
    ui->PrintText(Title, TextPos::Left);

    if (ui->getHeight() > 2) {
        ui->lcd->setCursor(0, ui->getHeight() / 2);
        ui->PrintText(Info, TextPos::Left);
    }

    const char *Options[] = {"> Aceptar <", "> Cancelar <", "> Reintentar <"};
    ui->lcd->setCursor(0, ui->getHeight() - 1);
    if (ui->Blinker.Update())
        ui->PrintText(Options[Index], TextPos::Center);
    else
        ui->PrintText("", TextPos::Left);

    UI_DialogResult res = UI_DialogResult::Cancel;
    Keys key = ui->GetKeys();
    switch (key) {
    case Keys::Enter:

    case Keys::Esc:
        if (Index == Ok)
            res = UI_DialogResult::Ok;
        else if (Index == OkCancel)
            res = UI_DialogResult::Cancel;
        else if (Index == OkCancelRetry)
            res = UI_DialogResult::Retry;

        ESP_LOGI(TAG, "question result: %s -> %i", Options[Index], res);

        Close(res);

        if (Function)
            Function(res);

        break;

    case Keys::Up:
        Index++;
        if (Index > static_cast<int32_t>(Opts))
            Index = static_cast<int32_t>(Opts);
        break;

    case Keys::Down:
        Index--;
        if (Index < 0)
            Index = 0;
        break;
    }
    return true;
}

bool Screen_Question::ShowQuestion(const char *Title, const char *Info)
{
    return ShowQuestion(Title, Info, OkCancel, nullptr);
}

bool Screen_Question::ShowQuestion(const char *Title, const char *Info, Options Opt)
{
    return ShowQuestion(Title, Info, Opt, nullptr);
}

bool Screen_Question::ShowQuestion(const char *Title, const char *Info, Options Opt, void (*Function)(UI_DialogResult))
{
    this->Title = Title;
    this->Info = Info;
    this->Opts = (Options)constrain(Opt, OK, OkCancelRetry);
    this->Function = Function;
    this->Index = Ok;

    return Show();
}

const char *Days[] = {"Dom", "Lun", "Mar", "Mie", "Jue", "Vie", "Sab"};
const char *Months[] = {"Ene", "Feb", "Mar", "Abr", "May", "Jun", "Jul", "Ago", "Sep", "Oct", "Nov", "Dic"};

bool Screen_Date::Run(lcd_ui *ui, UI_Action action)
{
    char str[20];
    if (action == UI_Action::Init || action == UI_Action::Restore) {
        ESP_LOGI(TAG, "action = %u", action);
        ui->lcd->clear();
        if (action == UI_Action::Init) {
            Index = 0;
            Changed = false;
        }
        return true;
    } else if (action == UI_Action::Closing) {
        ESP_LOGI(TAG, "action = %u", action);
        return true;
    } else if (action != UI_Action::Run) {
        ESP_LOGI(TAG, "action = %u", action);
        return false;
    }

    if (Changed == false) { // Mientras no se cambie ning�n campo actualizar en tiempo real
        if (TimeP == nullptr)
            Secs = time(nullptr); // Usar la hora del sistema
        else
            Secs = *TimeP;                // Actualizar con el puntero a la posici�n de memoria que tiene el dato
        Secs += Zone.tz_minuteswest * 60; // Ajusta por zona horaria
        Time = *gmtime(&Secs);            // Convertir a struct tm seg�n tiempo UTC
    }

    ui->lcd->setCursor(0, 0);
    ui->PrintText(Title, TextPos::Center);

    ui->lcd->setCursor(0, 1);
    sprintf(str, "%02i:%02i:%02i", Time.tm_hour, Time.tm_min, Time.tm_sec);
    if (Index < 3 && !ui->Blinker.Update()) {
        int x = Index * 3;
        str[x++] = ' ';
        str[x] = ' ';
    }
    ui->PrintText(str, TextPos::Center);

    ui->lcd->setCursor(0, 2);
    // Lun 1 abr 2020
    sprintf(str, "%s %i %s %04i", Days[Time.tm_wday], Time.tm_mday, Months[Time.tm_mon], Time.tm_year + 1900);
    if (!ui->Blinker.Update()) {
        int p = Time.tm_mday >= 9 ? 1 : 0;
        if (Index == 3) {
            str[4] = ' ';
            if (Time.tm_mday >= 9)
                str[5] = ' ';
        } else if (Index == 4) {
            str[6 + p] = ' ';
            str[7 + p] = ' ';
            str[8 + p] = ' ';
        } else if (Index == 5) {
            str[10 + p] = ' ';
            str[11 + p] = ' ';
            str[12 + p] = ' ';
            str[13 + p] = ' ';
        }
    }
    ui->PrintText(str, TextPos::Center);

    ui->lcd->setCursor(0, 3);
    ui->PrintText(Info, TextPos::Center);

    Keys key = ui->GetKeys();
    int32_t Inc = 0;
    switch (key) {
    case Keys::Next:
        Index++;
        if (Index > 5)
            Index = 0;
        break;

    case Keys::Enter:
        if (Changed) {
            if (TimeP) {
                Secs += Zone.tz_minuteswest * 60; // Ajusta por zona horaria
                *TimeP = Secs;
                *ZoneP = Zone;
            } else { // Hora del sistema
                struct timeval timeinfo = {Secs, 0};
                timeinfo.tv_sec += Zone.tz_minuteswest * 60; // Ajusta por zona horaria
                setTimeZone(&Zone);
                settimeofday(&timeinfo, nullptr);
            }
        }
        ui->Close(UI_DialogResult::Ok);
        break;

    case Keys::Esc:
        ui->Close(UI_DialogResult::Cancel);
        break;

    case Keys::Up:
        Inc = 1;
        break;

    case Keys::Down:
        Inc = -1;
        break;
    }

    if (Inc != 0) {
        Changed = true;
        if (Index == 0)
            Time.tm_hour += Inc;
        else if (Index == 1)
            Time.tm_min += Inc;
        else if (Index == 2)
            Time.tm_sec += Inc;
        else if (Index == 3)
            Time.tm_mday += Inc;
        else if (Index == 4)
            Time.tm_mon += Inc;
        else if (Index == 5)
            Time.tm_year += Inc;

        LimitFields(&Time); // Manejar el incremento de los campos
        Serial.println(&Time);

        Secs = timegm(&Time); // Volver a convertir en segundos UTC
        Serial.println(Secs);
        Serial.println(Secs);
        Time = *gmtime(&Secs); // Coordinar los segundos con la hora (UTC)
        Serial.println(&Time);
    }
    return true;
}

bool Screen_Date::ShowDlg(const char *Title, const char *Info)
{
    return ShowDlg(Title, Info, nullptr, nullptr);
}

bool Screen_Date::ShowDlg(const char *Title, const char *Info, time_t *Time)
{
    return ShowDlg(Title, Info, Time, nullptr);
}

bool Screen_Date::ShowDlg(const char *Title, const char *Info, time_t *Time, struct timezone *TimeZone)
{
    this->Title = Title;
    this->Info = Info;
    this->TimeP = Time;
    this->ZoneP = TimeZone; // Asignar la zona horaria
    if (ZoneP == nullptr) { // Si no hay puntero a zona horaria, cargar la zona horaria del sistema
        struct timeval timeinfo;
        gettimeofday(&timeinfo, &Zone);
        // Calcular el offset de la zona horaria
        time_t zero = 0;
        struct tm info = *localtime(&zero);
        Serial.println(&info);
        Zone.tz_minuteswest = timegm(&info) / 60;
        Zone.tz_dsttime = 0;

        // Serial.printf("Zona horaria: %i, %i\n", Zone.tz_minuteswest, Zone.tz_dsttime);
        // Serial.println(getenv("TZ"));
    } else
        Zone = *TimeZone;

    return Show();
}

void Screen_Date::setOnClose(void (*Function)(UI_DialogResult))
{
    this->Function = Function;
}

void Screen_Date::setUTCZone()
{
    Zone.tz_dsttime = 0;
    Zone.tz_minuteswest = 0;
    ZoneP = &Zone;
}

time_t Screen_Date::timegm(register struct tm *t)
{
    register long year;
    register time_t result;
#define MONTHSPERYEAR 12 /* months per calendar year */
    static const int cumdays[MONTHSPERYEAR] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

    /*@ +matchanyintegral @*/
    year = 1900 + t->tm_year + t->tm_mon / MONTHSPERYEAR;
    result = (year - 1970) * 365 + cumdays[t->tm_mon % MONTHSPERYEAR];
    result += (year - 1968) / 4;
    result -= (year - 1900) / 100;
    result += (year - 1600) / 400;
    if ((year % 4) == 0 && ((year % 100) != 0 || (year % 400) == 0) &&
        (t->tm_mon % MONTHSPERYEAR) < 2)
        result--;
    result += t->tm_mday - 1;
    result *= 24;
    result += t->tm_hour;
    result *= 60;
    result += t->tm_min;
    result *= 60;
    result += t->tm_sec;
    if (t->tm_isdst == 1)
        result -= 3600;
    /*@ -matchanyintegral @*/
    return (result);
}

char *Screen_Date::setTimeZone(int32_t offset, int32_t daylight)
{
    char cst[17] = {0};
    char cdt[17] = {0};
    static char tz[33] = {0};

    // Serial.printf("Zona horaria: %i, daylight :%i\n", offset, daylight);
    offset *= -1;

    if (offset % 3600) {
        sprintf(cst, "UTC%ld:%02u:%02u", offset / 3600, abs((offset % 3600) / 60), abs(offset % 60));
    } else {
        sprintf(cst, "UTC%ld", offset / 3600);
    }
    if (daylight) {
        long tz_dst = offset - daylight;
        if (tz_dst % 3600) {
            sprintf(cdt, "DST%ld:%02u:%02u", tz_dst / 3600, abs((tz_dst % 3600) / 60), abs(tz_dst % 60));
        } else {
            sprintf(cdt, "DST%ld", tz_dst / 3600);
        }
    }
    sprintf(tz, "%s%s", cst, cdt);
    // Serial.printf("Time Zone: %s\n", tz);
    setenv("TZ", tz, 1);
    tzset();
    return tz;
}

void Screen_Date::setTimeZone(timezone *TimeZone)
{
    Screen_Date::setTimeZone(TimeZone->tz_minuteswest * 60, TimeZone->tz_dsttime * 60);
}

void Screen_Date::LimitFields(tm *date)
{
    int32_t year = date->tm_year + 1900, max = 31;
    if (date->tm_mon == 2) {
        if (year % 400 == 0 || (year % 100 != 0 && year % 4 == 0))
            max = 29;
        else
            max = 28;
    } else if (date->tm_mon == 3 || date->tm_mon == 5 || date->tm_mon == 8 || date->tm_mon == 10) // Month es �ndice 0
        max = 30;

    date->tm_year = constrain_overrun(date->tm_year, 0, 137);
    date->tm_mon = constrain_overrun(date->tm_mon, 0, 11);
    date->tm_mday = constrain_overrun(date->tm_mday, 1, max);
    date->tm_hour = constrain_overrun(date->tm_hour, 0, 23);
    date->tm_min = constrain_overrun(date->tm_min, 0, 59);
    date->tm_sec = constrain_overrun(date->tm_sec, 0, 59);
}

void Screen_Date::TruncFields(tm *date)
{
    int32_t year = date->tm_year + 1900, max = 31;
    if (date->tm_mon == 2) {
        if (year % 400 == 0 || (year % 100 != 0 && year % 4 == 0))
            max = 29;
        else
            max = 28;
    } else if (date->tm_mon == 3 || date->tm_mon == 5 || date->tm_mon == 8 || date->tm_mon == 10) // Month es �ndice 0
        max = 30;

    date->tm_year = constrain(date->tm_year, 0, 137);
    date->tm_mon = constrain(date->tm_mon, 0, 11);
    date->tm_mday = constrain(date->tm_mday, 1, max);
    date->tm_hour = constrain(date->tm_hour, 0, 23);
    date->tm_min = constrain(date->tm_min, 0, 59);
    date->tm_sec = constrain(date->tm_sec, 0, 59);
}
