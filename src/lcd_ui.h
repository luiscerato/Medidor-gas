#ifndef __lcd_ui_h
#define __lcd_ui_h

#include "keyboard.h"
#include "lwip/apps/sntp.h"
#include "sound.h"
#include <LiquidCrystal.h>
#include <time.h>

using namespace std;

#define sizeof_menu(x) (sizeof(x) / (sizeof(x[0]))) // Calcula la cantidad de elementos de un men� (o un array)

#define constrain_overrun(amt, low, high) ((amt) < (low) ? (high) : ((amt) > (high) ? (low) : (amt))) // Limita un valor a determinado rango, pero si lo sobrepasa, va al l�mite opuesto (overrun)

#define UI_Max_Screens 32 // N�mero m�ximo de pantallas que se pueden contener
#define UI_Max_Levels 16  // Profundidad m�xima de niveles de pantallas

#define ID_UI_Black -10
#define ID_UI_SetVal -1
#define ID_UI_SetString -2
#define ID_UI_SetIP -3
#define ID_UI_SetTime -4

// forward-declare classes and structures
class lcd_ui; // Solo para proveer visivilidad a los objetos por encima de la implementacion de la clase.
class lcd_ui_screen;
// class OffsetIndex;
// class MenuHelper;
// class OffsetIndex;
// enum class ValType;
// enum class ValBase;
// class ValFormat_t;
// class MultiVal;
// class ValToString;
// template <typename T>class ValInfo;

/*
        Enumeraci�n del tipo de sonido que debe generar la interfase de usuario
*/
enum class UI_Sound
{
    None = 0, // Ning�n sonido es generado
    Beep,     // Se emite un sonido de pulsaci�n de tecla normal.
    Enter,    // Se genera un sonido cuando se presiona la tecla enter. (Entra a una pantalla o menu)
    Closing,  // Se genera un sonido cuando se presiona la tecla esc. (Vuelve atr�s una pantalla o menu)
};

/*
        Enumeraci�n del tipo de acci�n que debe ejecutar la funci�n de la pantalla que est� corriendo
*/
enum class UI_Action
{
    None = 0, // No debe realizar ninguna funci�n
    Init,     // Solo debe ejecutar funciones de inicio, es para preparar el entorno antes de ejecutar esa pantalla. Se llama cuando se ejecuta ui->Show();
    Closing,  // Solo debe ejecutar funciones para terminar la visualizaci�n de la pantalla. Se llama una vez que la pantalla se cierra, cuando se llama ui->Close();
    Restore,  // Se ejecuta antes de volver a ser visible la pantalla, es decir luego de llamar a ui->Close();
    Run       // La funci�n de la pantalla se ejecuta normalmente. En este caso puede realizar cualquier funci�n.
};

/*
        Enumeraci�n del tipo de resultado al cerrarse una pantalla.

        Mismos nombres y valores que VB.net
        https://docs.microsoft.com/en-us/dotnet/api/system.windows.forms.dialogresult?view=netcore-3.1
*/
enum class UI_DialogResult : int32_t
{
    None = 0, // No se devolvi� ning�n resultado
    Ok,       // Indicar que la pantalla se cerr� aceptando la opci�n seleccionada
    Cancel,   // Indicar que la pantalla se cerr� rechazando la opci�n seleccionada
    Abort,    // Indicar que la pantalla se cerr� seleecionando la opci�n 'ABORTAR'
    Retry,    // Indicar que la pantalla se cerr� seleecionando la opci�n 'REINTENTAR'
    Ignore,   // Indicar que la pantalla se cerr� seleecionando la opci�n 'IGNORAR'
    Yes,      // Indicar que la pantalla se cerr� seleecionando la opci�n 'SI'
    No,       // Indicar que la pantalla se cerr� seleecionando la opci�n 'NO'
};

enum class TextPos
{
    Left = 0,
    Center,
    Right
};

/*
        Estructura que contiene los datos necesarios para llamar a una pantalla.
*/
struct UI_Screen_t
{
    const char *Name; // Puntero a un nombre de la pantalla.
    int32_t ID;       // Identificador de la pantalla. En el caso de pantallas del sistema es menor a 0, para las de usuario es mayor a 0
    union
    {                                                // Uni�n de los dos tipos de funciones a llamar cuando est� corriendo la pantalla
        bool (*function)(lcd_ui *, UI_Action);       // Caller para la funci�n de la pantalla del usuario.
        bool (lcd_ui::*member)(lcd_ui *, UI_Action); // Caller para las funciones de pantalla del sistema.
    };
    lcd_ui_screen *ui_screen;

    enum Type
    {
        Undefined = 0,
        Function,
        Member,
        Class_Screen,
    };

    // Constructor
    UI_Screen_t()
    {
        init();
    };

    // Iniciar laestructura a los valores de defecto (devuelve isEmpty true)
    void init()
    {
        Name = nullptr;
        ID = 0;
        function = nullptr;
        ui_screen = nullptr;
    };

    // Chequea si la estructura est� inicializada a cero; no est� a puntando a ninguna pantalla. (est� vac�a)
    bool isEmpty()
    {
        if (Name == nullptr && ID == 0 && function == nullptr)
            return true;
        return false;
    };

    /*
            Llama a la funci�n que se quiere ejecutar, dependiendo del tipo de funci�n que es.
            Si ID > 0: ejecuta un llamada a funci�n (funci�n de usuario)
            Si ID < 0: ejecuta una llamada a un miembro de la clase lcd_ui, que deber�a ser una funci�n de pantalla del sistema
            Si ID == 0: no ejecuta ninguna llamada y devuelve false.

    */
    bool Execute(lcd_ui *ui, UI_Action action);

    Type getType()
    {
        if (ui_screen)
            return Class_Screen;
        else if (ID < 0)
            return Member;
        else if (ID > 0)
            return Function;
        return Undefined;
    };

    const char *ToString()
    {
        if (ui_screen)
            return "Class_Screen";
        else if (ID < 0)
            return "Member";
        else if (ID > 0)
            return "Function";
        return "Undefined";
    };
};

/*
        *****************Helpers****************

        Clases y funciones para simplificar y mejorar las funciones de la clase base
*/

/*
        class ValBlinker

        Es una clase que genera un pulso false-true a un intervalo de tiempo definido (seteable)
*/
class ValBlinker
{
private:
    uint32_t Time = 0;  // Marca de tiempo (millis())
    uint32_t Off = 500; // Tiempo en off en ms
    uint32_t On = 500;  // Tiempo en on en ms
    bool LastState = false;

    bool inline check()
    {
        if (!Off || !On)
            return 0;
        uint32_t elapsed = ((uint32_t)millis() - Time) % (Off + On);
        if (elapsed < Off)
            return false;
        return true; // else if (elapsed >= Off)
    };

public:
    // Constructor, tiempo por defecto 500ms de tiempo off y on
    ValBlinker()
    {
        Time = 0;
        Off = 500;
        On = 500;
    };

    // Constructor, tiempo seteable igual para off-on
    ValBlinker(uint32_t ms)
    {
        Time = 0;
        Off = On = ms;
    };

    // Constructor, tiempo seteable para off y on
    ValBlinker(uint32_t Off_ms, uint32_t On_ms)
    {
        Time = 0;
        Off = Off_ms;
        On = On_ms;
    };

    // Actualiza la marca de tiempo y devuelve en que estado est� la salida (false en tiempo off y true en tiempo on)
    bool Update()
    {
        return LastState = check();
    };

    bool HasChanged()
    {
        bool state = check();
        if (state != LastState) {
            LastState = state;
            return true;
        }
        LastState = state;
        return false;
    };

    // Devuelve el estado de la �ltima actualizaci�n
    inline bool getState()
    {
        return LastState;
    };

    // Resetea la marca temporal para que vuelva el estado off (false)
    void Reset()
    {
        Time = millis();
    };

    // Setea la marca temporal para que vuelva al estado on (true)
    void Set()
    {
        Time = (uint32_t)millis() - Off;
    };

    // Cambia los intervalos de tiempo a valores iguales para off-on, en ms
    // No resetea la marca temporal
    void SetTime(uint32_t ms)
    {
        Off = On = ms;
    };

    // Cambia los intervalos de tiempo a valores definibles para off y on, en ms
    // No resetea la marca temporal
    void SetTime(uint32_t Off_ms, uint32_t On_ms)
    {
        Off = Off_ms;
        On = On_ms;
    };

    // Indica si los dos tiempos son 0ms
    bool isZero()
    {
        return Off == 0 && On == 0;
    };
};

/*
        Esta clase es para simplificar el calculo de offset en una pantalla con varios
        items. En una pantalla de 4 lineas si se quieren mostrar 10 items y elegir uno
        de ellos al cambiar el elegido se van cambiando los que est�n en pantalla solo
        cuando sea necesario.
*/
class OffsetIndex
{
    int32_t Index;  // Indice seleccionado
    int32_t Offset; // Offset de la ventana
    int32_t Min;    // Valor m�nimo que puede tener index
    int32_t Max;    // Valor m�ximo que puede tener index
    int32_t WItems; // Cantidad de items que puede tener la ventana
    bool RollOver;  // Indica si se produce overflow o se mantiene en el l�mite

    void check()
    {
        if (Min > Max) {
            int32_t i = Max;
            Max = Min;
            Min = i;
        }
        if (Index < Min) {
            if (RollOver)
                Index = Max;
            else
                Index = Min;
        } else if (Index > Max) {
            if (RollOver)
                Index = Min;
            else
                Index = Max;
        }

        if (Index < Offset)
            Offset = Index;
        else if (Index > Offset + (WItems - 1))
            Offset = Index - (WItems - 1);
    };

public:
    OffsetIndex()
    {
        setParams(0, 1, 1, false);
    };

    OffsetIndex(int32_t IndexMin, int32_t IndexMax, int32_t WindowItems, bool DoRoolOver = true)
    {
        setParams(IndexMin, IndexMax, WindowItems);
        Index = IndexMin;
        Offset = IndexMin;
    };

    void setParams(int32_t IndexMin, int32_t IndexMax, int32_t WindowItems, bool DoRoolOver = true)
    {
        Min = IndexMin;
        Max = IndexMax;
        WItems = WindowItems;
        RollOver = DoRoolOver;
        check();
    };

    void Print()
    {
        Serial.printf("Min:%i, Max:%i, Win:%i, Index: %i, Offset: %i\n", Min, Max, WItems, Index, Offset);
    };

    int32_t getOffset()
    {
        check();
        return Offset;
    };

    inline int32_t getMax() { return Max; };
    inline int32_t getMin() { return Min; };
    inline int32_t getItemWindow() { return WItems; };

    int32_t operator++(int)
    {
        Index++;
        check();
        return Index;
    };

    int32_t operator--(int)
    {
        Index--;
        check();
        return Index;
    };

    // int32_t operator +(int32_t Val) {
    //	Index += Val;
    //	check();
    //	//Serial.printf("Index+= %i\n", Index);
    //	return Index;
    // };

    // int32_t operator -(int32_t Val) {
    //	Index -= Val;
    //	check();
    //	//Serial.printf("Index-= %i\n", Index);
    //	return Index;
    // };

    void operator=(int32_t Val)
    {
        Index = Val;
        check();
    };

    bool operator==(int32_t Val)
    {
        return Val == Index;
    };

    operator int() const
    {
        return Index;
    };
};

enum class ValType : int32_t
{
    Undefined = 0,
    Integer,
    UInteger,
    Float
};

enum class ValBase : int32_t
{
    //	Binary = 1,
    Octal = 2,
    Decimal,
    Hexadecimal
};

class InputStr_t
{
public:
    char *Src = nullptr;
    String Str = "";
    int32_t MaxCar = -1;
    // bool Number = true;
    // bool Letters = true;
    // bool Symbols = true;

    bool Save()
    {
        if (Src) {
            int n;
            if (MaxCar < 0)
                MaxCar = Str.length();
            else
                n = MaxCar <= Str.length() ? MaxCar - 1 : Str.length();

            strncpy(Src, Str.c_str(), n);
            Serial.printf("Str save() Max: %d, n:%d\n", MaxCar, n);
            if (MaxCar > 0) {
                for (; n < MaxCar; n++)
                    Src[n] = 0;
            }
            return true;
        }
        return false;
    };

    bool HasChanged()
    {
        if (Str == Src)
            return false;
        return true;
    };
};

class ValFormat_t
{
public:
    int32_t Width = 0;         // Ancho m�nimo del campo
    int32_t DecimalPoint = 0;  // Punto decimal
    ValBase Base;              // Base num�rica
    bool PositiveSign = false; // Mostrar signo positivo
    bool FillZeros;            // Llenar espacios con 0
    bool Prefix;               // Agregar prefijo de base num�rica
    char Unit[10];             // Unidad a mostrar (a la dereceha del valor)

    ValFormat_t()
    {
        setDefault();
    };

    ValFormat_t(char *Unit)
    {
        setDefault();
        setUnit(Unit);
    }

    ValFormat_t(int decimals)
    {
        setDefault();
        DecimalPoint = decimals;
    }

    ValFormat_t(char *Unit, int decimals)
    {
        setDefault();
        setUnit(Unit);
        DecimalPoint = decimals;
    }

    void setDefault()
    {
        Width = 0;
        DecimalPoint = 0;
        PositiveSign = false;
        FillZeros = false;
        Base = ValBase::Decimal;
        Prefix = false;
        Unit[0] = 0;
    };

    void Print()
    {
        Serial.printf("Format= Widht:%i, Decimal:%i, Base:%i, Sign:%i, Zero:%i, Prefix:%i\n",
                      Width, DecimalPoint, Base, PositiveSign, FillZeros, Prefix);
    };

    void setUnit(char *unit)
    {
        strncpy(Unit, unit, sizeof(Unit));
        Unit[sizeof(Unit) - 1] = 0;
    }
};

template <typename T>
class ValInfo
{
private:
    ValType Type = ValType::Undefined;
    T Val = 0, Min = 0, Max = 0, *Source = nullptr;
    bool Overflow = false;

    void check()
    {
        if (Min != Max) {
            if (Overflow) {
                if (Val > Max)
                    Val = Min;
                else if (Val < Min)
                    Val = Max;
            } else {
                if (Val > Max)
                    Val = Max;
                else if (Val < Min)
                    Val = Min;
            }
        }
    };

public:
    void setParams(T *Source)
    {
        setParams(Source, 0, 0, true);
    };

    void setParams(T *Source, T Min, T Max)
    {
        setParams(Source, Min, Max, true);
    };

    void setParams(T *Source, T Min, T Max, bool Overflow)
    {
        this->Source = Source;
        if (this->Source)
            Val = *Source;
        else
            Val = 0;
        setLimits(Min, Max);
        this->Overflow = Overflow;
        if (std::is_same<T, int32_t>::value)
            Type = ValType::Integer;
        else if (std::is_same<T, uint32_t>::value)
            Type = ValType::UInteger;
        else if (std::is_same<T, float>::value)
            Type = ValType::Float;
        else
            Type = ValType::Undefined;
    };

    bool Save()
    {
        if (Source)
            *Source = Val;
        else
            return false;
        return true;
    };

    void setLimits(T min, T max)
    {
        Min = min;
        Max = max;
    };

    void setOverflow(bool Overflow)
    {
        this->Overflow = Overflow;
    };

    T add(T Inc)
    {
        Val += Inc;
        check();
        return Val;
    };

    T substract(T Dec)
    {
        Val -= Dec;
        check();
        return Val;
    };

    T setVal(T NewVal)
    {
        Val = NewVal;
        return Val;
    };

    T getVal()
    {
        return Val;
    };

    T getMin()
    {
        return Min;
    };

    T getMax()
    {
        return Max;
    };

    ValType getType()
    {
        return Type;
    };

    // operator int() {
    //	return (int)Val;
    // };

    // operator float() {
    //	return (float)Val;
    // };

    void Print()
    {
        Serial.printf("Val: dir:%0.8x, val %i, min:%i, max:%i, type: %i\n", Source, Val, Min, Max, Type);
    };
};

class MultiVal
{
private:
    float Inc = 0.0;

public:
    union
    {
        ValType Type;
        ValInfo<int32_t> Int;
        ValInfo<uint32_t> UInt;
        ValInfo<float> Float;
    };

    MultiVal()
    {
        Type = ValType::Integer;
    };

    ValType getType()
    {
        return Type;
    };

    void Add(int32_t Inc)
    {
        if (Type == ValType::Integer)
            Int.add(Inc);
    };

    void Add(uint32_t Inc)
    {
        if (Type == ValType::UInteger)
            UInt.add(Inc);
    };

    void Add(float Inc)
    {
        if (Type == ValType::Float)
            Float.add(Inc);
    };

    void Substract(int32_t Inc)
    {
        if (Type == ValType::Integer)
            Int.substract(Inc);
    };

    void Substract(uint32_t Inc)
    {
        if (Type == ValType::UInteger)
            UInt.substract(Inc);
    };

    void Substract(float Inc)
    {
        if (Type == ValType::Float)
            Float.substract(Inc);
    };

    void SetInc(float Inc)
    {
        this->Inc = Inc;
    };

    float GetInc()
    {
        return Inc;
    };

    void AddInc(bool Clear = false)
    {
        if (Type == ValType::Float)
            Float.add(Inc);
        else if (Type == ValType::Integer)
            Int.add(static_cast<int32_t>(Inc));
        else if (Type == ValType::UInteger)
            UInt.add(static_cast<uint32_t>(Inc));

        Serial.printf("MultiVal.Add -> Inc: %f, type: %i\n", Inc, Type);

        if (Clear)
            Inc = 0.0;
    };

    void SubInc(bool Clear = false)
    {
        if (Type == ValType::Float)
            Float.substract(Inc);
        else if (Type == ValType::Integer)
            Int.substract(static_cast<int32_t>(Inc));
        else if (Type == ValType::UInteger)
            UInt.substract(static_cast<uint32_t>(Inc));

        Serial.printf("MultiVal.Sub -> Inc: %f, type: %i\n", Inc, Type);

        if (Clear)
            Inc = 0.0;
    };

    bool Save()
    {
        if (Type == ValType::Float)
            return Float.Save();
        else if (Type == ValType::Integer)
            return Int.Save();
        else if (Type == ValType::UInteger)
            return UInt.Save();
        return false;
    };

    operator int()
    {
        return Int.getVal();
    };

    operator unsigned int()
    {
        return UInt.getVal();
    };

    operator float()
    {
        return Float.getVal();
    };
};

class ValToString
{
private:
    const int32_t Divider[10] = {0, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000};
    char Result[32];    // Resultado de conversion
    ValFormat_t Format; // Formato de conversion
    int32_t StrWidht;   // Ancho del resultado a mostrar

    ValType Type; // Tipo de valor cargado
    union
    {
        int32_t Int;   // Como entero
        uint32_t UInt; // Como entero sin signo
        float Float;   // Como float
    } Val;

    int32_t Convert();

    int32_t FormatDest(char *dest, bool Blink);

public:
    ValToString(int32_t Widht);

    void init(int32_t Widht);

    void setFormat(ValFormat_t *fmt);

    int32_t Convert(float Val);
    int32_t Convert(float Val, char *dest, bool Blink);

    int32_t Convert(int32_t Val);
    int32_t Convert(int32_t Val, char *dest, bool Blink);

    int32_t Convert(uint32_t Val);
    int32_t Convert(uint32_t Val, char *dest, bool Blink);

    const char *getResult()
    {
        return Result;
    };
};

class AutoInc
{
    float End = 10.0, Init = 1.0;
    uint32_t LastTime, Counter = 0, Step = 10, TimeInc = 125, TimeStop = 1250;
    uint32_t Filter = 350, FilterTime;
    Keys LastKey = Keys::None;
    keyboard *Keyboard = nullptr;
    bool LastState = false;

public:
    AutoInc()
    {
        AutoInc(1.0, 10.0, 50);
        Serial.printf("AutoInc %lu\n", micros());
    };

    AutoInc(float Init, float End)
    {
        AutoInc(Init, End, 50);
    };

    AutoInc(float Init, float End, uint32_t Step)
    {
        setInitEnd(Init, End);
        setStep(Step);
        TimeInc = 125;
        TimeStop = 1250;
        Serial.printf("AutoInc %lu\n", micros());
    };

    void setTimes(uint32_t TimeInc, uint32_t TimeStop)
    {
        this->TimeInc = TimeInc > 500 ? 500 : (TimeInc < 50 ? 50 : TimeInc);
        this->TimeStop = TimeStop > 2500 ? 2500 : (TimeStop < 500 ? 500 : TimeStop);
    };

    void setInitEnd(float Init, float End)
    {
        if (End < Init) {
            this->End = Init;
            this->Init = End;
        } else {
            this->Init = Init;
            this->End = End;
        }
    };

    void setKeyboard(keyboard *keys)
    {
        Keyboard = keys;
    };

    void clearKeyboard()
    {
        Keyboard = nullptr;
    }

    void setStep(uint32_t StepInc)
    {
        Step = StepInc > 100 ? 100 : (StepInc < 2 ? 2 : StepInc);
    };

    void UpdateKey(Keys key)
    {
        // Serial.printf("millis() -> %lu, lastTime: %u\n", millis(), LastTime);
        uint32_t mils = static_cast<uint32_t>(millis()); // Tomar los millis en una variable de 32 bits
        uint32_t Time = mils - LastTime;

        if (Keyboard) {
            // bool State = false;
            // if (Keyboard->isKeyDown(Keys::Down) || Keyboard->isKeyDown(Keys::Up)) {
            //	State = true;
            //	if (!LastState) {
            //		LastState = true;
            //		LastTime = mils;
            //	}
            //	if (key == Keys::Down || key == Keys::Up) {
            //		if (Time < TimeInc)
            //			Counter++;
            //	}
            //	LastTime = 0;
            // }
            // else {
            //	if (LastTime) {
            //		if (Time > TimeStop)
            //			Counter = 0;
            //		LastTime = 0;
            //	}
            //	else
            //		LastTime = mils;
            // }
            // if (key != Keys::Down && key != Keys::Up && key != Keys::None)
            //	Counter = 0;

            bool State = false;
            if (Keyboard->isKeyDown(Keys::Down) || Keyboard->isKeyDown(Keys::Up))
                State = true;

            if (State != LastState) {
                LastState = State;
                LastTime = mils;
            }

            Time = mils - LastTime;
            if (key == Keys::Down || key == Keys::Up) {
                if (Time > TimeInc)
                    Counter++;
            } else if (key == Keys::None) {
                if (Time > TimeStop && State == false)
                    Counter = 0;
            } else
                Counter = 0;
        } else {
            if (key == Keys::Down || key == Keys::Up) {
                LastTime = mils;
                if (Time < TimeInc)
                    Counter++;
                else if (Time > TimeStop)
                    Counter = 0;
                // Serial.printf("AutoInc -> Counter: %i, Time: %i ms\n", Counter, Time);
            } else if (key != Keys::None)
                Counter = 0;
        }
    };

    float getInc()
    {
        uint32_t i = (Counter) / Step;
        if (i > 100)
            i = 100;
        // i = (Counter + 1) / (Step * powl(2, i));
        float x = Init * powf(10.0, i);
        if (x > End)
            x = End;
        return x;
    };

    float getInc(Keys key)
    {
        UpdateKey(key);
        return getInc();
    };

    void Print()
    {
        Serial.printf("AutoInc\n");
        Serial.printf("Inc: %f, End: %f, Step: %u\n", Init, End, Step);
        Serial.printf("Counter: %u, TimeInc: %u, TimeStop: %u\n", Counter, TimeInc, TimeStop);
    }
};

struct SetTimeContext
{
    time_t *TimeSrc = nullptr;
    uint32_t Pos = 0, Max;
    bool Save = true;
    union
    {
        struct
        {
            unsigned Update : 1;
            unsigned Absolute : 1;
            unsigned AMPM : 1;
            unsigned ShowHour : 1;
            unsigned ShowDlg : 1;
            unsigned ShowDay : 1;
            unsigned ShowTimeZone : 1;
            unsigned ShowDaylight : 1;
        };
        uint32_t Vals;
    } Flags;
    struct tm Time;
    OffsetIndex Index;
    uint8_t Elements[5] = {3, 3, 1, 1, 1};
};

/*
        lcd_ui_screen

        Clase base para la implementaci�n de nuevas pantallas.
*/

class lcd_ui_screen
{
    // static int32_t ID;
    int32_t MyID;

    lcd_ui *ui;

protected:
    // virtual bool Run(lcd_ui *ui);

    virtual bool Show();

public:
    /*
            M�todo principal.

            Cuando se redefina esta funci�n se debe incluir todo el c�digo de visualizaci�n.
            No se debe bloquear nunca la ejecuci�n del loop.
    */
    virtual bool Run(lcd_ui *ui, UI_Action action);

    lcd_ui_screen();

    ~lcd_ui_screen();

    virtual bool Close(UI_DialogResult Result);

    bool begin(lcd_ui *main_ui, const char *Name, int32_t ID);

    friend class lcd_ui;
};

class Screen_Message : public lcd_ui_screen
{
    String Title = "Mensaje";
    String Message = "";
    uint32_t Timeout, LastTime;

public:
    Screen_Message()
    {
        Timeout = 10000;
        LastTime = 0;
    };

    bool Run(lcd_ui *ui, UI_Action action);

    bool ShowMessage(char *Title, char *Msg);

    bool ShowMessage(char *Title, char *Msg, uint32_t Timeout);
};

struct Option
{
    int32_t Val;
    char *Name;
    char *Info;

    constexpr Option(int32_t Val, char *Name) : Val(Val), Name(Name), Info(nullptr){};

    constexpr Option(int32_t Val, char *Name, char *Info) : Val(Val), Name(Name), Info(Info){};

    // constexpr Option() :
    //	Val(0), Name(nullptr), Info(nullptr) {};

    Option()
    {
        this->Val = 0;
        this->Name = nullptr;
        this->Info = nullptr;
    };
};

class Screen_Option : public lcd_ui_screen
{
    String Title = "Seleccione";
    int32_t *MyVal;
    int32_t Count;
    Option *List;
    bool ToErase;

public:
    Screen_Option()
    {
        List = nullptr;
        MyVal = nullptr;
    };

    bool Run(lcd_ui *ui, UI_Action action);

    bool ShowList(char *Title, int32_t *myVal, const Option *Options, uint32_t Count);

    bool ShowList(char *Title, int32_t *myVal, int32_t *Val, char **Names, char **Info, uint32_t Count);
};

class Screen_Question : public lcd_ui_screen
{
public:
    enum Options
    {
        Ok = 0,
        OkCancel,
        OkCancelRetry
    };

private:
    const char *Title;
    const char *Info;
    Options Opts;
    int32_t Index;
    void (*Function)(UI_DialogResult);

public:
    Screen_Question()
    {
        Title = "Mensaje";
        Info = nullptr;
        Opts = OkCancel;
        Function = nullptr;
        Index = 0;
    };

    bool Run(lcd_ui *ui, UI_Action action);

    bool ShowQuestion(const char *Title, const char *Info);

    bool ShowQuestion(const char *Title, const char *Info, Options Opt);

    bool ShowQuestion(const char *Title, const char *Info, Options Opt, void (*Function)(UI_DialogResult));
};

class Screen_Date : public lcd_ui_screen
{
public:
    enum Options
    {
        Ok = 0,
        OkCancel,
        OkCancelRetry
    };

private:
    const char *Title;
    const char *Info;
    time_t *TimeP, Secs;
    struct tm Time;
    struct timezone Zone, *ZoneP;
    bool Changed;
    int32_t Index;
    void (*Function)(UI_DialogResult);

public:
    Screen_Date()
    {
        Title = "Fecha y hora";
        // Info = nullptr;
        // TimeP = nullptr;
        ZoneP->tz_dsttime = 0;
        ZoneP->tz_minuteswest = 0;
        Function = nullptr;
        Index = 0;
    };

    bool Run(lcd_ui *ui, UI_Action action);

    bool ShowDlg(const char *Title, const char *Info);

    bool ShowDlg(const char *Title, const char *Info, time_t *Time);

    bool ShowDlg(const char *Title, const char *Info, time_t *Time, struct timezone *TimeZone);

    void setOnClose(void (*Function)(UI_DialogResult));

    /*
            Usa timezone UTC
    */
    void setUTCZone();

    static time_t timegm(tm *t);

    static char *setTimeZone(int32_t offset, int32_t daylight);

    static void setTimeZone(struct timezone *TimeZone);

    static void LimitFields(struct tm *date);

    static void TruncFields(tm *date);
};

/*
        **********************************************************
        class lcd_ui
*/
class lcd_ui
{
    UI_Screen_t Screens[UI_Max_Screens];            // Pantallas
    int32_t Screen_Stack[UI_Max_Levels];            // Pila de niveles de pantallas
    int32_t Screen_Index = 0;                       // Nivel en la pila de pantallas
    UI_Sound SoundType = UI_Sound::None;            // Sonido a reproducir
    UI_DialogResult Result = UI_DialogResult::None; // Resultado de la �ltima funci�n Close();
    bool ClearScreenOnScreenSwitch = false;         // Indica si se debe borrar todo el lcd al cambiar de pantalla.

    String Title = "";
    MultiVal Val;
    AutoInc ValAutoInc = AutoInc(1.0, 10.0);
    ValFormat_t Format;
    InputStr_t StrVal;
    int32_t NewID = 10000;

    IPAddress *IPs;
    IPAddress *IPsrc;
    uint32_t *IPIntsrc;
    int32_t IPCount = 0;

    SetTimeContext SetTime;

    uint64_t LastTime;                         //�ltima vez que se actualiz�
    uint32_t UpdateTime = 100;                 // Tiempo de actualizaci�n de UI
    static const uint32_t DftUpdateTime = 100; // Tiempo de actualizaci�n por defecto

    void ResetUpdateTime();
    bool GoBack = false;
    bool GoHome = false;
    bool InFunct = false;

    bool ShowByIndex(int32_t Index);

    struct
    {
        int32_t Widht;
        int32_t Height;
    } LCD;

    bool UI_SetVal(lcd_ui *ui, UI_Action action);

    bool UI_SetIP(lcd_ui *ui, UI_Action action);

    bool UI_SetStr(lcd_ui *ui, UI_Action action);

    bool UI_Black(lcd_ui *ui, UI_Action action);

    Sound Speaker;

    bool UsingKeyEnter = true;
    bool UsingKeyTab = true;
    bool UsingKeyEsc = true;

public:
    // lcd_ui(int32_t w, int32_t h);

    lcd_ui(int32_t w, int32_t h, int8_t SoundPin = -1);

    ValBlinker Blinker; //

    ~lcd_ui();

    inline int32_t getNewID() { return NewID++; };

    inline int32_t getWidht() { return LCD.Widht; };

    inline int32_t getHeight() { return LCD.Height; };

    LiquidCrystal *lcd = nullptr; // Puntero a LCD usada

    keyboard *keys = nullptr; // Puntero a teclado

    Screen_Message Msg;

    Screen_Option OptionBox;

    Screen_Question Question;

    // Screen_Date DateTime;

    void Using_KeyEnter(bool Using) { UsingKeyEnter = Using; };

    void Using_KeyTab(bool Using) { UsingKeyTab = Using; };

    void Using_KeyEsc(bool Using) { UsingKeyEsc = Using; };

    void begin(LiquidCrystal *lcd, keyboard *keys);

    /*
            Indica cual es la pantalla principal. Esta pantalla se muestra cuando no queda
            ninguna pantalla en la pila de pantallas o se llama Home();
    */
    bool setMainScreen(const char *UI);

    /*
            Ejecuta las tareas del stack de la interfaz de usuario
    */
    void Run();

    int32_t GetScreenIndex();

    /*
            Muestra otra pantalla por nombre.

            Usando esta funci�n se agrega la pantalla solicitada a la pila de pantallas y se la muestra

            Antes de que esta pantalla se ejecute se la llama con la action==Init.

            Si no se encuentra la pantalla no se hace nada.
    */
    bool Show(const char *UI);

    /*
            Muestra otra pantalla por �ndice.

            Usando esta funci�n se agrega la pantalla solicitada a la pila de pantallas y se la muestra

            Antes de que esta pantalla se ejecute se la llama con la action==Init

            Si no se encuentra la pantalla no se hace nada.
    */
    bool Show(int32_t UI_ID);

    /*
            Termina la ejecuci�n de la pantalla actual y vuelve a mostrar la pantalla anterior.

            Se pasa el resultado de la ejecuci�n, para que la anterior pueda recuperarlo, y as� determinar
            el cusro de ejecuci�n.
    */
    bool Close(UI_DialogResult result);

    /*
            Vuelve a la pantalla principal, terminando la ejecuci�n de todas las pantallas en la pila.

            Cuando se llame a cada pantalla se lo hace con la action==Closing
    */
    bool Home();

    /*
            Devulve el valor con el que se cerr� la pantalla anterior.
    */
    UI_DialogResult getDialogResult() { return Result; };

    /*
            Selecciona el tiempo de refresco de la pantalla actual.
    */
    void SetUpdateTime(uint32_t Ms = DftUpdateTime);

    /*
            Consulta al teclado y devuelve la �ltima tecla presionada.

            Tambi�n, dependiendo el c�digo de la tecla, actualiza el valor de Blinker.
    */
    Keys GetKeys();

    /*
            Indica si cada vez que se hace un cambio de pantalla se debe borrar el LCD

            Se da un cambio de pantalla cada vez que se llama Show() o Close()
    */
    void setClearScreenOnScreenSwitch(bool val) { ClearScreenOnScreenSwitch = val; };

    /*
            Devuelve el estado de la funci�n ClearScreenOnScreenSwitch.
    */
    bool getClearScreenOnScreenSwitch() { return ClearScreenOnScreenSwitch; };

    /*
            Escribe un texto en pantalla con la alineaci�n indicada.

            Widht indica el ancho m�ximo que se puede ocupar. Si == -1 ocupa todo el ancho de la pantalla.

            Antes de llamar esta funci�n se debe posicionar el cursor en la ubicai�n deseada.
    */
    void PrintText(const char *Text, TextPos Pos, int32_t Widht = -1);

    int32_t Add_UI(int32_t ID, const char *Name, bool (*func)(lcd_ui *, UI_Action));

    int32_t Add_UI(int32_t ID, const char *Name, bool (lcd_ui::*member)(lcd_ui *, UI_Action));

    int32_t Add_UI(int32_t ID, const char *Name, lcd_ui_screen &ui_screen);

    int32_t Remove_UI(const char *Name);

    int32_t Remove_UI(int32_t ID);

    // Set val INT version

    bool Show_SetVal(const char *Title, int32_t *Val);

    bool Show_SetVal(const char *Title, int32_t *Val, int32_t Min, int32_t Max);

    bool Show_SetVal(const char *Title, int32_t *Val, int32_t Min, int32_t Max, ValFormat_t &Format);

    // Set val UINT version

    bool Show_SetVal(const char *Title, uint32_t *Val, uint32_t Min, uint32_t Max, ValFormat_t &Format);

    bool Show_SetVal(const char *Title, uint32_t *Val, uint32_t Min, uint32_t Max);

    bool Show_SetVal(const char *Title, uint32_t *Val);

    // Set val Float version
    bool Show_SetVal(const char *Title, float *Val, float Min, float Max, ValFormat_t &Format);

    bool Show_SetVal(const char *Title, float *Val, float Min, float Max);

    bool Show_SetVal(const char *Title, float *Val);

    bool Show_SetIPAddress(const char *Title, uint32_t *IPs, int32_t Count);

    bool Show_SetString(const char *Title, char *Src, uint32_t MaxLenght = 32);

    bool Show_SetIPAddress(const char *Title, IPAddress *IPs, int32_t Count);

    void PrintState();

    /*
            Imprime una lista de todas las pantallas disponibles
    */
    void PrintScreenList();
};

/*
        class MenuHelper

        Esta clase permite simplificar la creaci�n de men�es a la m�nima expresi�n.
        Solo se debe definir el t�tulo, pasar un array con los items a mostrar y la cantidad de ellos.
        Luego se llama Run(), esta funci�n se encarga de mostrar las opciones disponibles, cuando se presiona
        Keys::Enter se guarda el �tem seleccionado y en la funci�n principal se puede consultar cual se seleccin�
        llamando a getItem(), que va a devolver en �ndice de base 1 la opci�n seleccionada.
*/
class MenuHelper
{
    lcd_ui *ui;
    char Title[22];
    char **Items;
    int32_t Count;
    OffsetIndex Index;
    const int32_t LcdWidht = 20;
    int32_t SelectedItem = -1;

public:
    MenuHelper()
    {
        MenuHelper("Menu", (const char **)nullptr, 0);
    };

    MenuHelper(char *MenuItems[], int32_t ItemsCount)
    {
        MenuHelper("Menu", MenuItems, ItemsCount);
    };

    MenuHelper(char *Title, char **MenuItems, int32_t ItemsCount)
    {
        setTitle(Title);
        setItems(MenuItems, ItemsCount);
    };

    MenuHelper(char *Title, const char **MenuItems, int32_t ItemsCount)
    {
        setTitle(Title);
        setItems((char **)MenuItems, ItemsCount);
    };

    void setTitle(char *Title)
    {
        strncpy(this->Title, Title, 21);
        this->Title[21] = 0;
    };

    bool setItems(char **MenuItems, int32_t ItemsCount)
    {
        Items = MenuItems;
        Serial.printf("MenuItems= 0x%.8X\n", MenuItems);
        if (ItemsCount < 0 || ItemsCount > 100)
            return false;
        Index.setParams(1, ItemsCount, 1, true);
        SelectedItem = -1;
        Index = 1;
        return true;
    };

    bool Run(lcd_ui *ui, UI_Action action)
    {
        this->ui = ui;
        if (action == UI_Action::Init) {
            Index = 1;
            SelectedItem = -1;

            return true;
        } else if (action == UI_Action::Run) {
            ui->lcd->setCursor(0, 0);
            int32_t space = ((int)Index < 10 ? 1 : 0) + (Index.getMax() < 10 ? 1 : 0);
            int32_t lenght = LcdWidht - (5 - space) - 1; // Calcular ancho disponible para el titulo
            // Imprimir el t�tulo a la derecha, rellenar espacios, y a la izquierda el n�mero de items
            ui->lcd->printf("%-*.*s %u/%u", lenght, lenght, Title, (int)Index, Index.getMax());

            for (int i = 0; i < Index.getItemWindow(); i++) {
                ui->lcd->setCursor(0, 1 + i); // Posicionar cursor en la l�nea correspondiente

                if (i + Index.getOffset() <= Index.getMax() && Items != nullptr) {
                    bool Selected = i + Index.getOffset() == (int)Index;
                    // Serial.printf("loop i:%u, offset:%u, dir: 0x%.8X -> \n", i, Index.getOffset(), Items[i + Index.getOffset() - 1]);
                    ui->lcd->printf("%c%-*.*s", Selected ? '>' : ' ', LcdWidht - 1, LcdWidht - 1,                // Si est� seleccionado imprimir un '>' sino espacio
                                    !ui->Blinker.Update() && Selected ? " " : Items[i + Index.getOffset() - 1]); // Si est� seleccionado y debe parpadear, imprimir espacios, sino el nombre del item
                } else
                    ui->lcd->printf("%-*.*s", LcdWidht, LcdWidht, " "); // L�nea en blanco (fin de lista)
            }

            switch (ui->GetKeys()) {
            case Keys::Enter:
            case Keys::Next:
                SelectedItem = Index;
                break;
            case Keys::Esc:
                ui->Close(UI_DialogResult::Cancel);
                break;
            case Keys::Up:
                Index++;
                break;
            case Keys::Down:
                Index--;
                break;
            }
        }
        return true;
    };

    int32_t getItem()
    {
        int32_t i = SelectedItem;
        SelectedItem = -1;
        return i;
    };
};

#endif
