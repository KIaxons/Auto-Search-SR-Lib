#include "pch.h"
#include "shlobj.h"
#include <string>
#include <iomanip>
//Для конвертации wstring в string
#include <locale>
#include <codecvt>
//Для работы с массивами типа vector
#include <vector>
//Инклюдим заголовки с основными элементами оригинальной игровой структуры
#include "../../CustomFunctions/CustomFunctions/structs/include.h"
#include "../../CustomFunctions/CustomFunctions/wrapped-classes/include.h"
#include "../../CustomFunctions/CustomFunctions/TList_methods.cpp"
//Там есть много полезных готовых функций
#include "../../CustomFunctions/CustomFunctions/dllmain.h"

//using namespace std;

WCHAR system_folder[MAX_PATH];
WCHAR log_path[MAX_PATH];

//Объявляем проверочную переменную
static bool was_init = false;
//Объявляем кастомный тип для ячеек массива
struct Bonus {
    wstring BonusNumInCfg = L"";
    wstring Name = L"";
    wstring Equipments = L"";
    int ImitatedHullType = 0;
    int ImitatedEquipType = 0;
    wstring ImitatedEquipStringType = L"";
    wstring AcrynSerie = L"";
    int AcrynLevel = 0;
    wstring SerieLevel1 = L"";
    wstring SerieLevel2 = L"";
    wstring SerieLevel3 = L"";
    wstring SerieLevel4 = L"";
    wstring SerieLevel5 = L"";
    wstring SerieLevel6 = L"";
    wstring SerieLevel7 = L"";
    wstring SerieLevel8 = L"";
    int LevelsCount = 0;
    int bonHull = 0;
    int bonSpeed = 0;
    int bonJump = 0;
    int bonRadar = 0;
    int bonScan = 0;
    int bonDroid = 0;
    int bonHook = 0;
    int bonDef = 0;
    wstring WeaponMods = L"";
    int NoDeltaMod = 0;
    int NonSearchable = 0;
};
//Присваиваем тип будущему массиву
static Bonus* BonusArray = nullptr;

//Объявляем будущий массив дамагсетов типов оригинальных видов оружия (из самого предмета оружия их, к сожалению, не получить)
static int* MainWeaponTypeDamageSet = nullptr;

//Хранилище указателей на игровые сектора
static unsigned int Sectors[20];

//Создаём структуру данных запроса
struct {
    int ItemType = 0;
    int Race[8] = { 0,0,0,0,0,0,0,0 };
    int HullType[11] = { 0,0,0,0,0,0,0,0,0,0,0 };
    int MainDamageType[3] = { 0,0,0 }; //Основные типы урона орудия Э/О/Р
    wstring CustomWeaponType = L"";
    int Param1 = 0;
    int Param2 = 0;
    int Param3 = 0;
    int AcrynNum = 0;
    int AcrynLevel = 0;
    int ItemSize = 0;
    int ItemCost = 0;
    int SkipDicea = 0;
} Request;

//Объявляем массив для возврата успешных результатов
struct Result {
    unsigned int Item = 0;
    unsigned int Shop = 0;
};
static vector <Result> SuccessfulResults;

//Для записи всех необходимых функций внутрь самой библиотеки необходимо использовать следующую настройку:
//Configuration Properties > C/C++ > Code Generation > Runtime Library > Multi - threaded(/ MT) (для релиза)
//Configuration Properties > C/C++ > Code Generation > Runtime Library > Multi - threaded Debug(/ MTd) (для дебага)
//Подробнее: https://stackoverflow.com/questions/3162325/after-building-exe-using-vs-2010-c-missing-msvcp100-dll

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
)
{
    switch(ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
        {
            SHGetSpecialFolderPathW(0, system_folder, CSIDL_PERSONAL, true);
            wcscpy(log_path, system_folder);
            wcscat(log_path, L"\\SpaceRangersHD\\########.log");
            break;
        }
        case DLL_THREAD_ATTACH: break;
        case DLL_THREAD_DETACH: break;
        case DLL_PROCESS_DETACH:
        {
            if(was_init)
            {
                delete[] BonusArray;
                BonusArray = nullptr;

                delete[] MainWeaponTypeDamageSet;
                MainWeaponTypeDamageSet = nullptr;

                was_init = false;
            }
            break;
        }
    }
    return TRUE;
}

//Функции для пользования внутри либы
//Используется для вывода строки в игровой лог,
//могут возникать проблемы с кириллическими символами
//(для конверта числа в строку использовать to_string())
void SFT(const wstring& str_out)
{
    //const wchar_t* out = str_out.c_str();
    const char* out = ws2s(str_out).c_str(); //Необходимо переводить в обычный string, поскольку лог выводится в UTF-8
    //"Безопасный" вариант от майков под комментарием
    //FILE* file;
    //errno_t err = fopen_s(&file, log_path, "a");

    FILE* file = _wfopen(log_path, L"a");
    fwrite(out, strlen(out), 1, file);
    fwrite("\n", 1, 1, file);
    fclose(file);
}

//Банковское округление
int BankRound(float value)
{
    float fShift = (value >= 0 ? 0.5f : -0.5f);
    // Проверяем среднее значение для округления
    if(fabs(fabs(value) - fabs(float(int(value))) - 0.5) < DBL_EPSILON)
    {
        return int(float(int(value / 2.0 + fShift) * 2));
    }
    return int(float(int(value + fShift)));
}

bool IsSpecialAgent(TShip* ship)
{
    if(ship == Player) return false;

    TScriptShip* ship_script_object = ship->script_ship;
    if(!ship_script_object) return false;

    TScript* script = ship_script_object->script;
    if(wcscmp(script->name, L"Script.PC_fem_rangers")) return false; //wcscmp() возвращает 0, если строки равны

    TScriptGroup* group = (TScriptGroup*)script->groups->items[ship_script_object->group];
    if(wcscmp(group->name, L"GroupFem")) return false;

    return true;
}

//Возвращает кастомный тип корабля (для всех ванильных не изменённых скриптом типов будет возвращать "")
wstring ShipType(TShip* ship)
{
    wchar_t* type = ship->custom_type_name;
    if(type) return type;
    //Возврат строки типа для ванильных кораблей (по умолчанию она "")
    else
    {
        switch(ShipTypeN(ship))
        {
            case(t_Kling): return L"Kling";
            case(t_Ranger):
            {
                if(IsSpecialAgent(ship)) return L"FemRanger";
                return L"Ranger";
            }
            case(t_Transport):
            {
                switch(ShipSubType(ship))
                {
                    case(0): return L"Transport";
                    case(1): return L"Liner";
                    case(2): return L"Diplomat";
                }
            }
            case(t_Pirate): return L"Pirate";
            case(t_Warrior):
            {
                switch(ShipSubType(ship))
                {
                    case(0): return L"Warrior";
                    case(1): return L"WarriorBig";
                }
            }
            case(t_Tranclucator): return L"Tranclucator";
            case(t_RC): return L"RC";
            case(t_PB): return L"PB";
            case(t_WB):
            {
                if(ship == Player->cur_bridge) return L"PlayerBridge";
                else return L"WB";
            }
            case(t_SB): return L"SB";
            case(t_BK): return L"BK";
            case(t_MC): return L"MC";
            case(t_CB): return L"CB";
            case(t_UB):
            {
                SFT(L"UtilityFunctions.dll ShipType function error. Wrong ruin t_UB ShipType determination.");
                throw 0;
            }
        }
        //На всякий случай
        return L"";
    }
}
//Функция, возвращающая тип указанного предмета
int ItemType(uint32_t item)
{
    return ((TItem*)item)->type;
    //return *(char*)(item + 12);
}
//Функция, возвращающая стоимость указанного предмета
int ItemCost(uint32_t item)
{
    return ((TItem*)item)->cost;
    //return *(int*)(item + 32);
}
//Функция, возвращающая вес указанного предмета
int ItemSize(uint32_t item)
{
    return ((TItem*)item)->size;
    //return *(int*)(item + 24);
}
//Функция, возвращающая расу производителя указанного предмета
int ItemOwner(uint32_t item)
{
    return ((TItem*)item)->race;
    //return *(char*)(item + 28);
}
//Функция, возвращающая текущий процент износа предмета (0 - полностью изношенный предмет)
int ItemDurability(uint32_t item)
{
    return (int)((TEquipment*)item)->durability;
    //return *(double*)(item + 72);
}
//Функция, проверяющая, не сломан ли предмет
bool IsBroken(uint32_t item)
{
    return ((TEquipment*)item)->is_broken;
    //return *(char*)(item + 80);
}
//Функция, возвращающая кастомную расу указанного предмета
wstring EqCustomFaction(uint32_t item)
{
    //wchar_t *str = *(wchar_t**)(item + 60);
    wchar_t* str = ((TEquipment*)item)->custom_faction;
    if(str) return str;
    else return L"";
}
//Функция, возвращающая номер бонуса микромодуля указанного предмета
int EqModule(uint32_t item)
{
    return ((TEquipment*)item)->mm - 1;
    //return *(int*)(item + 88) - 1;
}
//Функция, возвращающая номер акрина указанного предмета
int EqSpecial(uint32_t item)
{
    return ((TEquipment*)item)->acryn - 1;
    //return *(int*)(item + 92) - 1;
}
//Функция, возвращающая тип корпуса для указанного корпуса
int HullType(uint32_t hull)
{
    return ((THull*)hull)->type;
}
//Функция, возвращающая серию корпуса для указанного корпуса
int HullSeries(uint32_t hull)
{
    return ((THull*)hull)->serie;
    //return *(int*)(hull + 120);
}
//Функция, возвращающая текущую броню корпуса
int HullArmor(uint32_t hull)
{
    return ((THull*)hull)->armor;
}
//Функция, возвращающая текущее число единиц прочности корпуса
int HullHP(dword hull)
{
    return ((THull*)hull)->hitpoints;
    //return *(int*)(hull + 112);
}
//Функция, возвращающая текущий процент повреждения корпуса (0 - полностью целый корпус)
int HullDamage(dword hull)
{
    return 100 - BankRound(((float)HullHP(hull) / ItemSize(hull)) * 100);
}
//Функция, возвращающая урон и дальность для оружия
int GetWeaponData(dword weapon, int data_num)
{
    if(!data_num) return ((TWeapon*)weapon)->min_dmg;
    else if(data_num == 1) return ((TWeapon*)weapon)->max_dmg;
    else return ((TWeapon*)weapon)->radius;
}
//Функция, возвращающая минимальный урон оружия с учётом возможного наличия на оружии модификатора NoDelta
int GetMinWeaponDmgWithNoDeltaCheck(dword weapon, int weapon_type)
{
    //Если оружие оригинальное
    //То начинаем уныло перебирать все возможные навешенные бонусы
    if(weapon_type < t_CustomWeapon)
    {
        //Если на оружие наложен акрин
        int itemAcryn = EqSpecial(weapon);
        if(itemAcryn > -1)
        {
            if(BonusArray[itemAcryn].NoDeltaMod) return GetWeaponData(weapon, 1);
        }

        //Или в него установлен ММ
        int itemModule = EqModule(weapon);
        if(itemModule > -1)
        {
            if(BonusArray[itemModule].NoDeltaMod) return GetWeaponData(weapon, 1);
        }

        TList* extra_acryns = ((TEquipment*)weapon)->extra_acryns;
        //Если на оружии имеются какие-либо экстраакрины
        if(extra_acryns) //Списка может не существовать (будет равно null)
        {
            for(int i = 0; i < extra_acryns->count; ++i)
            {
                int itemAcryn = *(int*)(extra_acryns->items[i]) - 1; //В массив здесь записан не сам номер, а указатель на номер акрина :|
                //int cnt = *(int*)(exbonuses->items[i] + 4); //Строка для получения количества конкретных экстраакринов bon на предмете
                if(BonusArray[itemAcryn].NoDeltaMod) return GetWeaponData(weapon, 1);
            }
        }

        //Если так и не нашли модификатора NoDelta, возвращаем стандартный минимальный урон
        return GetWeaponData(weapon, 0);
    }
    //Если кастомное
    //То нам будет достаточно просто проверить дамагсет
    else
    {
        dword winfo = ((TCustomWeapon*)weapon)->custom_weapon_info;
        unsigned int damage_set = *(unsigned int*)(winfo + 49);

        if(damage_set & 1048576) return GetWeaponData(weapon, 1);
        else return GetWeaponData(weapon, 0);
    }
}
//Функция, возвращающая строчный тип указанного кастомного оружия
wchar_t* CustomWeaponType(dword weapon)
{
    dword winfo = ((TCustomWeapon*)weapon)->custom_weapon_info;
    return *(wchar_t**)(winfo + 4); //Название кастомного типа
}
//Функция для возврата основного типа урона (Э/О/Р) указанного оружия
int GetWeaponMainDamageType(
    int weapon_type,
    uint32_t weapon
)
{
    //Для оригинального орудия берём значение из записанного ранее массива базовых дамагсетов оригинальных типов орудий
    unsigned int damage_set = 0;
    if(weapon_type < t_CustomWeapon)
    {
        damage_set = MainWeaponTypeDamageSet[weapon_type - t_Weapon1];
    }
    //Для кастомного орудия сперва определяем его текущий дамагсет
    else
    {
        uint32_t winfo = ((TCustomWeapon*)weapon)->custom_weapon_info;
        damage_set = *(unsigned int*)(winfo + 49);
    }
    if(damage_set & 1) return 0; //Энергетический
    if(damage_set & 2) return 1; //Осколочный
    if(damage_set & 4) return 2; //Ракетный
    return 0; //На случай, если в дамагсете орудия не указан базовый тип урона (что, в принципе, есть ошибка)
}
//Функция, возвращающая характеристики двигателя
int GetEngineData(uint32_t item, int data_num)
{
    if(!data_num) return ((TEngine*)item)->speed;
    else return ((TEngine*)item)->parsec;
}
//Функция, возвращающая характеристики топливного бака
int GetFuelTankData(uint32_t item)
{
    return ((TFuelTanks*)item)->capacity;
    //return *(unsigned char*)(item + 120);
}
//Функция, возвращающая характеристики радара
int GetRadarData(uint32_t item)
{
    return ((TRadar*)item)->radius;
    //return *(int*)(item + 116);
}
//Функция, возвращающая характеристики сканера
int GetScannerData(uint32_t item)
{
    return ((TScaner*)item)->power;
    //return *(signed char*)(item + 113);
}
//Функция, возвращающая характеристики дроида
int GetDroidData(uint32_t item)
{
    return ((TRepairRobot*)item)->recover_hit_points;
    //return *(unsigned char*)(item + 113);
}
//Функция, возвращающая характеристики захвата
int GetGripperData(uint32_t item, int data_num)
{
    if(!data_num) return ((TCargoHook*)item)->pick_up_size;
    else return ((TCargoHook*)item)->radius;

    //if(data_num == 0) return *(int*)(item + 116); //Размер захватываемых объектов
    //else return *(int*)(item + 120); //Дальность
}
//Функция, возвращающая характеристики ГЗП
int GetShieldGeneratorData(uint32_t item)
{
    return int(100.0 * (1.0 - ((TDefGenerator*)item)->def_factor));
    //return 100.0 * (1.0 - (*(float*)(item + 116)));
}

//Функция для проверки инициализации массивов
extern "C" __declspec(dllexport)
int IsAutoSearchDLLInit()
{
    if(was_init) return 1;
    else return 0;
}
//Функция, инициализирующая массивы бонусов и дамагсетов оригинальных орудий, а также задающая их длину
//Исполняется один раз при инициализации/реинициализации игры или изменении состава модов
extern "C" __declspec(dllexport)
void InitAutoSearchDLL(
    int size,
    TPlayer* player
)
{
    //Наконец, объявляем сам массив (размер равен числу доступных бонусов в игре)
    BonusArray = new Bonus[size];
    MainWeaponTypeDamageSet = new int[18]; //18 оригинальных типов орудий в игре
    Player = player;

    was_init = true;
}

////Функция, занимающаяся автоматической загрузкой игровых секторов по указателю
//void LoadSector(unsigned int sector)
//{
//    int no = *(int*)(sector + 4) - 1;
//    if(Sectors[no]) return;
//    Sectors[no] = sector;
//    TList& lst = **(TList**)(sector + 28);
//    for(int i = 0; i < lst.count; ++i) LoadSector(lst.items[i]);
//}
////Функция для загрузки игровых галактических секторов в dll
////Необходимо передавать в неё любой сектор прямо перед выполнением серии (или единичного) запросов
//extern "C" __declspec(dllexport)
//void LoadSectorsToAutoSearchLib(unsigned int sector)
//{
//    for(int i = 0; i < 20; ++i) Sectors[i] = 0;
//    LoadSector(sector);
//}

//Функция, записывающая в BonusArray всю необходимую информацию об игровом бонусе под указанным номером
extern "C" __declspec(dllexport)
void FillAutoSearchBonusArray(
    int BonNum,
    wchar_t* BonusNumInCfg,
    wchar_t* Name,
    wchar_t* Equipments,
    wchar_t* ImitatedHullType,
    int ImitatedEquipType,
    wchar_t* ImitatedEquipStringType,
    wchar_t* AcrynSerie,
    int AcrynLevel,
    wchar_t* SerieLevel1,
    wchar_t* SerieLevel2,
    wchar_t* SerieLevel3,
    wchar_t* SerieLevel4,
    wchar_t* SerieLevel5,
    wchar_t* SerieLevel6,
    wchar_t* SerieLevel7,
    wchar_t* SerieLevel8,
    int LevelsCount,
    int bonHull,
    int bonSpeed,
    int bonJump,
    int bonRadar,
    int bonScan,
    int bonDroid,
    int bonHook,
    int bonDef,
    wchar_t* WeaponMods,
    int NonSearchable
)
{
    if(BonusNumInCfg != nullptr) BonusArray[BonNum].BonusNumInCfg = BonusNumInCfg;
    if(Name != nullptr) BonusArray[BonNum].Name = Name;
    if(Equipments != nullptr) BonusArray[BonNum].Equipments = Equipments;
    if(ImitatedHullType != nullptr)
    {
        wstring StrHullTypes = ImitatedHullType;
        if(StrHullTypes.find(L"Ranger") != wstring::npos) BonusArray[BonNum].ImitatedHullType += 1;
        if(StrHullTypes.find(L"Warrior") != wstring::npos) BonusArray[BonNum].ImitatedHullType += 2;
        if(StrHullTypes.find(L"Pirate") != wstring::npos) BonusArray[BonNum].ImitatedHullType += 4;
        if(StrHullTypes.find(L"Transport") != wstring::npos) BonusArray[BonNum].ImitatedHullType += 8;
        if(StrHullTypes.find(L"Liner") != wstring::npos) BonusArray[BonNum].ImitatedHullType += 16;
        if(StrHullTypes.find(L"Diplomat") != wstring::npos) BonusArray[BonNum].ImitatedHullType += 32;
        if(StrHullTypes.find(L"Flagman") != wstring::npos) BonusArray[BonNum].ImitatedHullType += 2048;
    }
    BonusArray[BonNum].ImitatedEquipType = ImitatedEquipType;
    if(ImitatedEquipStringType != nullptr) BonusArray[BonNum].ImitatedEquipStringType = ImitatedEquipStringType;
    if(AcrynSerie != nullptr) BonusArray[BonNum].AcrynSerie = AcrynSerie;
    BonusArray[BonNum].AcrynLevel = AcrynLevel;
    if(SerieLevel1 != nullptr) BonusArray[BonNum].SerieLevel1 = SerieLevel1;
    if(SerieLevel2 != nullptr) BonusArray[BonNum].SerieLevel2 = SerieLevel2;
    if(SerieLevel3 != nullptr) BonusArray[BonNum].SerieLevel3 = SerieLevel3;
    if(SerieLevel4 != nullptr) BonusArray[BonNum].SerieLevel4 = SerieLevel4;
    if(SerieLevel5 != nullptr) BonusArray[BonNum].SerieLevel5 = SerieLevel5;
    if(SerieLevel6 != nullptr) BonusArray[BonNum].SerieLevel6 = SerieLevel6;
    if(SerieLevel7 != nullptr) BonusArray[BonNum].SerieLevel7 = SerieLevel7;
    if(SerieLevel8 != nullptr) BonusArray[BonNum].SerieLevel8 = SerieLevel8;
    BonusArray[BonNum].LevelsCount = LevelsCount;
    BonusArray[BonNum].bonHull = bonHull;
    BonusArray[BonNum].bonSpeed = bonSpeed;
    BonusArray[BonNum].bonJump = bonJump;
    BonusArray[BonNum].bonRadar = bonRadar;
    BonusArray[BonNum].bonScan = bonScan;
    BonusArray[BonNum].bonDroid = bonDroid;
    BonusArray[BonNum].bonHook = bonHook;
    BonusArray[BonNum].bonDef = bonDef;
    if(WeaponMods != nullptr)
    {
        BonusArray[BonNum].WeaponMods = WeaponMods;
        if(BonusArray[BonNum].WeaponMods.find(L"NoDelta") != wstring::npos) BonusArray[BonNum].NoDeltaMod = 1;
    }
    BonusArray[BonNum].NonSearchable = NonSearchable;
}
//Функция для заполнения массива MainWeaponTypeDamageSet
extern "C" __declspec(dllexport)
void FillAutoSearchDamageSetArray(
    int weapon_type,
    int damage_set
)
{
    MainWeaponTypeDamageSet[weapon_type - t_Weapon1] = damage_set;
}
//Функция для заполнения структуры запроса перед запуском цикла поиска предметов
extern "C" __declspec(dllexport)
void FillAutoSearchRequestList(
    wchar_t* ReqType,
    int RaceMaloc,
    int RacePeleng,
    int RacePeople,
    int RaceFei,
    int RaceGaal,
    int RaceKling,
    int RaceNone,
    int RacePirate,
    int Param1,
    int Param2,
    int Param3,
    int Param4,
    int Param5,
    int Param6,
    int Param7,
    int Param8,
    int WeaponType,
    wchar_t* CustomWeaponType,
    int AcrynNum,
    int AcrynLevel,
    int ItemSize,
    int ItemCost,
    int SkipDicea
)
{
    wstring RequestType = ReqType;
    //Запоминаем выданный скриптом маркер на пропуск скрытого пиратского сектора
    Request.SkipDicea = SkipDicea;
    if(RequestType == L"Weapons")
    {
        Request.ItemType = WeaponType; //Числовой тип оружия
        if(CustomWeaponType != nullptr) Request.CustomWeaponType = CustomWeaponType;//Кастомный тип оружия
        Request.Race[0] = RaceMaloc;
        Request.Race[1] = RacePeleng;
        Request.Race[2] = RacePeople;
        Request.Race[3] = RaceFei;
        Request.Race[4] = RaceGaal;
        Request.Race[5] = RaceKling;
        Request.Race[6] = RaceNone;
        Request.Race[7] = RacePirate;
        Request.Param1 = Param4; //Минимальный уровень минимальных повреждений оружия
        Request.Param2 = Param5; //Минимальный уровень максимальных повреждений оружия
        Request.Param3 = Param6; //Минимальная дальность стрельбы
        Request.MainDamageType[0] = Param1; //Доступен ли для выбора энергетический тип оружия
        Request.MainDamageType[1] = Param2; //Доступен ли для выбора осколочный тип оружия
        Request.MainDamageType[2] = Param3; //Доступен ли для выбора ракетный тип оружия
        Request.AcrynNum = AcrynNum;
        Request.AcrynLevel = AcrynLevel;
        Request.ItemSize = ItemSize;
        Request.ItemCost = ItemCost;
    }
    else if(RequestType == L"Hulls")
    {
        Request.ItemType = t_Hull;
        Request.Race[0] = RaceMaloc;
        Request.Race[1] = RacePeleng;
        Request.Race[2] = RacePeople;
        Request.Race[3] = RaceFei;
        Request.Race[4] = RaceGaal;
        Request.Race[5] = RaceKling;
        Request.Race[6] = RaceNone;
        Request.Race[7] = RacePirate;
        Request.HullType[0] = Param1; //Доступен ли для выбора рейнджерский корпус
        Request.HullType[1] = Param2; //Доступен ли для выбора корпус военного
        Request.HullType[2] = Param3; //Доступен ли для выбора пиратский корпус
        Request.HullType[3] = Param4; //Доступен ли для выбора корпус транспорта
        Request.HullType[4] = Param5; //Доступен ли для выбора корпус дипломата
        Request.HullType[5] = Param6; //Доступен ли для выбора корпус лайнера
        Request.HullType[10] = Param7; //Доступен ли для выбора корпус флагмана
        Request.Param1 = Param8; //Минимальная броня корпуса
        Request.AcrynNum = AcrynNum;
        Request.AcrynLevel = AcrynLevel;
        Request.ItemSize = ItemSize;
        Request.ItemCost = ItemCost;
    }
    else if(RequestType == L"Engines")
    {
        Request.ItemType = t_Engine;
        Request.Race[0] = RaceMaloc;
        Request.Race[1] = RacePeleng;
        Request.Race[2] = RacePeople;
        Request.Race[3] = RaceFei;
        Request.Race[4] = RaceGaal;
        Request.Race[5] = RaceKling;
        Request.Race[6] = RaceNone;
        Request.Race[7] = RacePirate;
        Request.Param1 = Param1; //Скорость двигателя
        Request.Param2 = Param2; //Дальность прыжка
        Request.AcrynNum = AcrynNum;
        Request.AcrynLevel = AcrynLevel;
        Request.ItemSize = ItemSize;
        Request.ItemCost = ItemCost;
    }
    else if(RequestType == L"FuelTanks")
    {
        Request.ItemType = t_FuelTanks;
        Request.Race[0] = RaceMaloc;
        Request.Race[1] = RacePeleng;
        Request.Race[2] = RacePeople;
        Request.Race[3] = RaceFei;
        Request.Race[4] = RaceGaal;
        Request.Race[5] = RaceKling;
        Request.Race[6] = RaceNone;
        Request.Race[7] = RacePirate;
        Request.Param1 = Param1; //Ёмкость бака
        Request.AcrynNum = AcrynNum;
        Request.AcrynLevel = AcrynLevel;
        Request.ItemSize = ItemSize;
        Request.ItemCost = ItemCost;
    }
    else if(RequestType == L"Radars")
    {
        Request.ItemType = t_Radar;
        Request.Race[0] = RaceMaloc;
        Request.Race[1] = RacePeleng;
        Request.Race[2] = RacePeople;
        Request.Race[3] = RaceFei;
        Request.Race[4] = RaceGaal;
        Request.Race[5] = RaceKling;
        Request.Race[6] = RaceNone;
        Request.Race[7] = RacePirate;
        Request.Param1 = Param1; //Дальность радара
        Request.AcrynNum = AcrynNum;
        Request.AcrynLevel = AcrynLevel;
        Request.ItemSize = ItemSize;
        Request.ItemCost = ItemCost;
    }
    else if(RequestType == L"Scanners")
    {
        Request.ItemType = t_Scaner;
        Request.Race[0] = RaceMaloc;
        Request.Race[1] = RacePeleng;
        Request.Race[2] = RacePeople;
        Request.Race[3] = RaceFei;
        Request.Race[4] = RaceGaal;
        Request.Race[5] = RaceKling;
        Request.Race[6] = RaceNone;
        Request.Race[7] = RacePirate;
        Request.Param1 = Param1; //Мощность сканера
        Request.AcrynNum = AcrynNum;
        Request.AcrynLevel = AcrynLevel;
        Request.ItemSize = ItemSize;
        Request.ItemCost = ItemCost;
    }
    else if(RequestType == L"Droids")
    {
        Request.ItemType = t_RepairRobot;
        Request.Race[0] = RaceMaloc;
        Request.Race[1] = RacePeleng;
        Request.Race[2] = RacePeople;
        Request.Race[3] = RaceFei;
        Request.Race[4] = RaceGaal;
        Request.Race[5] = RaceKling;
        Request.Race[6] = RaceNone;
        Request.Race[7] = RacePirate;
        Request.Param1 = Param1; //Эффективность дроида
        Request.AcrynNum = AcrynNum;
        Request.AcrynLevel = AcrynLevel;
        Request.ItemSize = ItemSize;
        Request.ItemCost = ItemCost;
    }
    else if(RequestType == L"Grippers")
    {
        Request.ItemType = t_CargoHook;
        Request.Race[0] = RaceMaloc;
        Request.Race[1] = RacePeleng;
        Request.Race[2] = RacePeople;
        Request.Race[3] = RaceFei;
        Request.Race[4] = RaceGaal;
        Request.Race[5] = RaceKling;
        Request.Race[6] = RaceNone;
        Request.Race[7] = RacePirate;
        Request.Param1 = Param1; //Размер захватываемых объектов
        Request.AcrynNum = AcrynNum;
        Request.AcrynLevel = AcrynLevel;
        Request.ItemSize = ItemSize;
        Request.ItemCost = ItemCost;
    }
    else if(RequestType == L"ShieldGenerators")
    {
        Request.ItemType = t_DefGenerator;
        Request.Race[0] = RaceMaloc;
        Request.Race[1] = RacePeleng;
        Request.Race[2] = RacePeople;
        Request.Race[3] = RaceFei;
        Request.Race[4] = RaceGaal;
        Request.Race[5] = RaceKling;
        Request.Race[6] = RaceNone;
        Request.Race[7] = RacePirate;
        Request.Param1 = Param1; //Процент блокируемого урона
        Request.AcrynNum = AcrynNum;
        Request.AcrynLevel = AcrynLevel;
        Request.ItemSize = ItemSize;
        Request.ItemCost = ItemCost;
    }
}

//Функция для проверки предмета на соответствие условиям запроса
//Автоматически заносит предмет в массив результатов при успехе проверки
void CheckItemSuitability(
    dword item,
    dword shop
)
{
    //Проверка на случай, если массив с бонусами по какой-то причине не был инициализирован
    if(!BonusArray)
    {
        SFT(L"Error in AutoSearch.dll: BonusArray was not initialised!");
        throw 0;
    }

    int itemType = ItemType(item);

    //Если акрин на данном предмете является имитацией другого типа предмета, то меняем искомый тип на тип, имитирующий искомый
    int itemAcryn = EqSpecial(item);
    bool was_weapon_replace = false;
    bool was_type_replace = false;
    if(itemAcryn > -1)
    {
        //Выставляем маркера, обозначающие замену типов
        if(BonusArray[itemAcryn].ImitatedEquipType)
        {
            //Если оружие меняется на другое оружие, то будет достаточно простого маркера о подмене для активации доп. проверки кастомного типа
            if(itemType >= t_Weapon1)
            {
                itemType = BonusArray[itemAcryn].ImitatedEquipType;
                if(itemType < t_Weapon1) was_type_replace = true;
                else was_weapon_replace = true;
            }
            //Если же тип предмета меняется радикально (двигатель на радар, например), то выставляем маркер полной замены,
            //Который отключает все профильные проверки для конкретных типов оборудования или оружия (скорость, дальность и т.д.)
            else
            {
                if(BonusArray[itemAcryn].ImitatedEquipType != itemType)
                {
                    itemType = BonusArray[itemAcryn].ImitatedEquipType;
                    was_type_replace = true;
                }
            }
        }
    }

    //Особая проверка для поиска любого вида оружия
    if(Request.ItemType == -1)
    {
        //Сперва с ходу отсеиваем всё, что не является оружием
        if(itemType < t_Weapon1 || itemType > t_CustomWeapon) return;
        //Затем проверяем, соответствует ли тип урона проверяемого оружия запросу
        if(!Request.MainDamageType[GetWeaponMainDamageType(itemType, item)]) return;
        itemType = -1;
    }

    if(itemType == Request.ItemType || itemType == -1)
    {
        //Проверка на размер
        if(Request.ItemSize)
        {
            if(itemType != t_Hull)
            {
                if(ItemSize(item) > Request.ItemSize) return;
            }
            else
            {
                if(ItemSize(item) < Request.ItemSize) return;
            }
        }

        //Проверка на стоимость
        if(Request.ItemCost)
        {
            if(ItemCost(item) > Request.ItemCost) return;
        }

        //Проверка на расу-производителя
        if(Request.Race[ItemOwner(item)] != 1) return;

        //Исключаем любые предметы с кастомной фракцией, кроме тех, что имеют сугубо декоративную фракцию
        if(EqCustomFaction(item) != L"")
        {
            if(EqCustomFaction(item).substr(0, 10) != L"SubFaction") return;
        }

        //Проверка на соответствие искомого акрина/его отсутствия
        int tint = 0;
        //int itemAcryn = EqSpecial(item);
        //Если задано условие на поиск конкретного акрина
        if(Request.AcrynNum >= 0)
        {
            //Сразу исключаем предметы без акрина
            if(itemAcryn < 0) return;
            //Исключённые из поиска акрины могут имитировать предметы без акрина
            if(BonusArray[itemAcryn].NonSearchable == 2) return;
            //Если установлен фильтр по уровню акрина
            if(Request.AcrynLevel)
            {
                //Если серии акринов не совпадают, либо акрин на предмете вовсе не имеет серии
                if(BonusArray[itemAcryn].AcrynSerie != BonusArray[Request.AcrynNum].AcrynSerie) return;
                //Если фильтр уровня выставлял не дебил
                if(Request.AcrynLevel > 1)
                {
                    //Если уровень акрина на предмете оказался слишком мал
                    if(BonusArray[itemAcryn].AcrynLevel < Request.AcrynLevel) return;
                }
            }
            //Если на предмет наложен неверный акрин
            else if(itemAcryn != Request.AcrynNum)
            {
                //Также проверяем соответствие серий (в запросе может быть указан серийный акрин)
                if(BonusArray[Request.AcrynNum].AcrynSerie != L"")
                {
                    if(BonusArray[itemAcryn].AcrynSerie != BonusArray[Request.AcrynNum].AcrynSerie) return;
                }
                else return;
            }
        }
        //Если задано условие на поиск любого акрина или его отсутствия
        else if(Request.AcrynNum == -1)
        {
            if(itemAcryn < 0)
            {
                //Если имеется фильтр на уровень акрина, расцениваем это как обязательное условие наличия хоть какого-нибудь акрина
                if(Request.AcrynLevel) return;
            }
            else
            {
                if(Request.AcrynLevel)
                {
                    //В случае, если установлен фильтр на поиск любого (в т.ч. первого) уровня акрина, расцениваем его как обязательное условие наличия любого акрина
                    //В таком случае предмет с акрином, имитирующим его отсутствие, нам не подойдёт
                    if(BonusArray[itemAcryn].NonSearchable == 2) return;
                    //Если задан поиск конкретного уровня акрина
                    if(Request.AcrynLevel > 1)
                    {
                        //Определяем уровень акрина на предмете
                        //Если уровень акрина на предмете оказался слишком мал
                        if(BonusArray[itemAcryn].AcrynLevel < Request.AcrynLevel) return;
                    }
                }
            }
        }
        //Если задано условие на поиск предмета без акрина
        else if(Request.AcrynNum == -2)
        {
            //Если на предмет наложен акрин
            if(itemAcryn >= 0)
            {
                //Если акрин на предмете не имитирует отсутствие акрина
                if(BonusArray[itemAcryn].NonSearchable != 2) return;
            }
        }

        //Теперь переходим к более частным проверкам (если проверяемый тип не был заменён акрином)
        if(!was_type_replace)
        {
            //Если ищем оружие
            if((itemType >= t_Weapon1 && itemType <= t_CustomWeapon) || itemType == -1)
            {
                //Проверяем, соответствует ли данное кастомное оружие искомому (если ищем кастомку)
                if(itemType == t_CustomWeapon)
                {
                    if(CustomWeaponType(item) != Request.CustomWeaponType)
                    {
                        //Если кастомный тип оружия не соответствует запросу, то проверяемый помимо него имитируемый акрином кастомный тип
                        if(was_weapon_replace)
                        {
                            if(itemAcryn > -1)
                            {
                                if(BonusArray[itemAcryn].ImitatedEquipStringType != Request.CustomWeaponType) return;
                            }
                        }
                        else return;
                    }
                }

                //Проверяем максимальный урон (GetWeaponData возвращает все значения сразу с учётом бонусов)
                if(Request.Param2)
                {
                    tint = GetWeaponData(item, 1);
                    if(tint < Request.Param2) return;
                }

                //Проверяем дальнобойность (GetWeaponData возвращает все значения сразу с учётом бонусов)
                if(Request.Param3)
                {
                    tint = GetWeaponData(item, 2);
                    if(tint < Request.Param3) return;
                }

                //Оставляем проверку минимального урона на конец, т.к. она самая долгая
                //Проверяем минимальный урон (GetWeaponData возвращает все значения, кроме учёта модификатора NoDelta)
                if(Request.Param1)
                {
                    if(itemType == -1) itemType == ItemType(itemType);
                    tint = GetMinWeaponDmgWithNoDeltaCheck(item, itemType);
                    if(tint < Request.Param1) return;
                }
            }
            //Если ищем корпус
            else if(itemType == t_Hull)
            {
                //Проверка на минимальную броню
                if(Request.Param1)
                {
                    if(itemAcryn >= 0) tint = HullArmor(item) + BonusArray[itemAcryn].bonHull;
                    else tint = HullArmor(item);
                    if(tint < Request.Param1) return;
                }
                //Проверяем оригинальный тип корпуса только если он не является акриновым, т.к. все акриновые корпуса имеют одинаковый акриновый тип
                if(itemAcryn == -1)
                {
                    tint = HullType(item);
                    //Если корпус выбивается из доступного диапазона, то прерываем поиск
                    if(tint > 5) return;
                    if(!Request.HullType[tint]) return;
                }
                //Если же корпус имеет акрин, то проверяем, не является ли он "имитатором" под обычный тип корпуса
                else
                {
                    //И если выяснилось, что является, то проверяем, соответствует ли имитируемый этим корпусом тип (или типы) запросу
                    if(BonusArray[itemAcryn].ImitatedHullType)
                    {
                        //Проверяем через заранее выписанный из акрина сет
                        bool found = false;
                        for(tint = 0; tint <= 10; ++tint)
                        {
                            if(!Request.HullType[tint]) continue;
                            if(BonusArray[itemAcryn].ImitatedHullType & (1 << tint))
                            {
                                found = true;
                                break;
                            }
                        }
                        if(!found) return;
                    }
                }
            }
            //Если ищем двигатель
            else if(itemType == t_Engine)
            {
                //Проверка на скорость двигателя
                if(Request.Param1)
                {
                    if(itemAcryn >= 0) tint = GetEngineData(item, 0) + BonusArray[itemAcryn].bonSpeed;
                    else tint = GetEngineData(item, 0);
                    if(tint < Request.Param1) return;
                }
                //Проверка на дальность прыжка
                if (Request.Param2)
                {
                    if(itemAcryn >= 0) tint = GetEngineData(item, 1) + BonusArray[itemAcryn].bonJump;
                    else tint = GetEngineData(item, 1);
                    if(tint < Request.Param2) return;
                }
            }
            //Если ищем топливный бак
            else if(itemType == t_FuelTanks)
            {
                //Проверка на ёмкость бака
                //Не проверяем бонус от акрина, т.к. расширять вместимость баков могут только микромодули
                if(Request.Param1)
                {
                    tint = GetFuelTankData(item);
                    if(tint < Request.Param1) return;
                }
            }
            //Если ищем радар
            else if(itemType == t_Radar)
            {
                //Проверка дальности радара
                if(Request.Param1)
                {
                    if(itemAcryn >= 0) tint = GetRadarData(item) + BonusArray[itemAcryn].bonRadar;
                    else tint = GetRadarData(item);
                    if(tint < Request.Param1) return;
                }
            }
            //Если ищем сканер
            else if(itemType == t_Scaner)
            {
                //Проверка мощности сканера
                if(Request.Param1)
                {
                    if(itemAcryn >= 0) tint = GetScannerData(item) + BonusArray[itemAcryn].bonScan;
                    else tint = GetScannerData(item);
                    if(tint < Request.Param1) return;
                }
            }
            //Если ищем дроида
            else if(itemType == t_RepairRobot)
            {
                //Проверка эффективности дроида
                if(Request.Param1)
                {
                    if(itemAcryn >= 0) tint = GetDroidData(item) + BonusArray[itemAcryn].bonDroid;
                    else tint = GetDroidData(item);
                    if(tint < Request.Param1) return;
                }
            }
            //Если ищем захват
            else if(itemType == t_CargoHook)
            {
                //Проверка максимального размера захватываемых объектов
                if(Request.Param1)
                {
                    if(itemAcryn >= 0) tint = GetGripperData(item, 0) + BonusArray[itemAcryn].bonHook;
                    else tint = GetGripperData(item, 0);
                    if(tint < Request.Param1) return;
                }
            }
            //Если ищем ГЗП
            else
            {
                //Проверка минимального процента блокировки урона
                if(Request.Param1)
                {
                    if(itemAcryn >= 0) tint = GetShieldGeneratorData(item) + BonusArray[itemAcryn].bonDef;
                    else tint = GetShieldGeneratorData(item);
                    if(tint < Request.Param1) return;
                }
            }
        }

        //Если дошли до сюда, заносим предмет в массив успешных результатов
        SuccessfulResults.push_back({ item, shop });
    }
    else return;
}
//Функция по перебору всех магазинных предметов в Галактике
//Вызывает внутри себя функцию непосредственной проверки предмета на соответствие запросу
extern "C" __declspec(dllexport)
int AutoSearchLaunch(TGalaxy* galaxy, TStar* player_star)
{
    //Предварительно очищаем предыдущие результаты поиска
    SuccessfulResults.clear();
    for(int i = 0; i < galaxy->stars->count; ++i)
    {
        //TStar* star = (TStar*)galaxy->stars->items[i]; //Перебор от нулевого номера
        TStar* star = player_star->stars[i].star; //Перебор звёзд по удалённости от текущей (под нулевым номером она сама)
        TConstellation* sector = star->sector;
        //Пропускаем скрытый пиратский сектор, если в запросе установлен соответствующий флаг
        if(Request.SkipDicea)
        {
            if(sector->id == 20) continue;
        }
        //Пропускаем закрытые для игрока сектора (вроде оригинальный поиск их не скипает)
        //if(!sector->is_visible) continue;
        //Перебор всех планет в секторе
        //Перед началом перебора планет исключаем системы с кастомной расой
        wchar_t* custom_faction = star->custom_faction;
        if(!custom_faction || custom_faction[0] == L'\0')
        {
            for(int k = 0; k < star->planets->count; ++k)
            {
                TPlanet* planet = (TPlanet*)star->planets->items[k];
                unsigned char owner = planet->owner;
                //Пропускаем доминаторские и незаселённые планеты
                if(owner == 5 || owner == 6) continue;
                //Перебор всех предметов в магазине планеты
                for(int u = 0; u < planet->equipment_store_items->count; ++u) CheckItemSuitability(planet->equipment_store_items->items[u], (uint32_t)planet);
            }
        }
        //Перебор всех станций в системе
        for(int k = 0; k < star->ships->count; ++k)
        {
            TShip* ship = (TShip*)star->ships->items[k];
            unsigned char ship_type = ship->type;
            //Пропускаем не станции
            if(ship_type < 6 || ship_type > 13) continue;
            if(ShipType(ship) == L"SSB") continue;
            //Перебор всех предметов в магазине станции
            TRuins* ruin = (TRuins*)ship;
            for(int u = 0; u < ruin->equipment_shop->count; ++u) CheckItemSuitability(ruin->equipment_shop->items[u], (uint32_t)ruin);
        }
    }

    return SuccessfulResults.size();
}

//Функция, возвращающая значения успешных результатов в игру
extern "C" __declspec(dllexport)
unsigned int GetAutoSearchResult(int result_num, int result_type)
{
    if(!result_type) return SuccessfulResults[result_num].Item;
    else return SuccessfulResults[result_num].Shop;
}