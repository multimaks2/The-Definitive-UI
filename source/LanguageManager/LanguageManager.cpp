/*****************************************************************************
 *
 *  PROJECT:     The-Definitive-UI
 *  FILE:        source/LanguageManager/LanguageManager.cpp
 *  PURPOSE:     UI / zone dictionaries
 *
 *****************************************************************************/

#include "LanguageManager.h"

#include "plugin.h"
#include "C_PcSave.h"
#include "CMenuManager.h"
#include "Config.h"
#include "HelpGxt.h"

#include <windows.h>
#include <cstdio>
#include <cstring>
#include <vector>
#include <cctype>

LanguageManager::LanguageManager() = default;
LanguageManager::~LanguageManager() = default;

namespace
{
    // Default Russian; EFIGS + Portuguese + Brazilian also available
    LanguageManager::Lang g_lang = LanguageManager::Lang::Russian;

    struct Loc8
    {
        const char* key;
        const char* t[static_cast<int>(LanguageManager::Lang::Count)];
    };

    // UI dictionary — American, French, German, Italian, Spanish, Russian, Portuguese, Brazilian
    const Loc8 kUi[] = {
        { "LANG_AMERICAN", { "American", "American", "American", "American", "American", "American", "American", "American" } },
        { "LANG_FRENCH",   { "Français", "Français", "Français", "Français", "Français", "Français", "Français", "Français" } },
        { "LANG_GERMAN",   { "Deutsch", "Deutsch", "Deutsch", "Deutsch", "Deutsch", "Deutsch", "Deutsch", "Deutsch" } },
        { "LANG_ITALIAN",  { "Italiano", "Italiano", "Italiano", "Italiano", "Italiano", "Italiano", "Italiano", "Italiano" } },
        { "LANG_SPANISH",  { "Español", "Español", "Español", "Español", "Español", "Español", "Español", "Español" } },
        { "LANG_RUSSIAN",  { "Русский", "Русский", "Русский", "Русский", "Русский", "Русский", "Русский", "Русский" } },

        { "LANG_PORTUGUESE", { "Português", "Português", "Português", "Português", "Português", "Português", "Português", "Português" } },
        { "LANG_BRAZILIAN",  { "Português (Brasil)", "Português (Brasil)", "Português (Brasil)", "Português (Brasil)", "Português (Brasil)", "Português (Brasil)", "Português (Brasil)", "Português (Brasil)" } },


        { "UI_ON",  {"On", "Oui", "Ein", "Sì", "Sí", "Вкл.", "Ligado", "Ligado"} },
        { "UI_OFF", {"Off", "Non", "Aus", "No", "No", "Выкл.", "Desligado", "Desligado"} },

        { "UI_START",     {"Start", "Commencer", "Start", "Inizia", "Empezar", "Начать", "Começar", "Começar"} },
        { "UI_CONTINUE",  {"Continue", "Continuer", "Fortsetzen", "Continua", "Continuar", "Продолжить", "Continuar", "Continuar"} },
        { "UI_GAME",      {"Game", "Jeu", "Spiel", "Gioco", "Juego", "Игра", "Jogo", "Jogo"} },
        { "UI_SETTINGS",  {"Settings", "Options", "Optionen", "Impostazioni", "Ajustes", "Настройки", "Definições", "Configurações"} },
        { "UI_EXIT",      {"Exit", "Quitter", "Beenden", "Esci", "Salir", "Выйти", "Sair", "Sair"} },
        { "UI_MAP",       {"Map", "Carte", "Karte", "Mappa", "Mapa", "Карта", "Mapa", "Mapa"} },
        { "UI_MESSAGES",  {"Messages", "Messages", "Nachrichten", "Messaggi", "Mensajes", "Сообщения", "Mensagens", "Mensagens"} },
        { "UI_STATS",     {"Stats", "Stats", "Statistik", "Statistiche", "Estadísticas", "Статистика", "Estatísticas", "Estatísticas"} },

        { "UI_SAVE",      {"SAVE", "SAUVER", "SPEICHERN", "SALVA", "GUARDAR", "СОХРАНИТЬ", "GUARDAR", "SALVAR"} },
        { "UI_LOAD",      {"LOAD GAME", "CHARGER", "LADEN", "CARICA", "CARGAR", "ЗАГРУЗИТЬ ИГРУ", "CARREGAR JOGO", "CARREGAR JOGO"} },
        { "UI_NEW_GAME",  {"NEW GAME", "NOUVELLE PARTIE", "NEUES SPIEL", "NUOVA PARTITA", "NUEVA PARTIDA", "НОВАЯ ИГРА", "NOVO JOGO", "NOVO JOGO"} },
        { "UI_DELETE",    {"DELETE GAME", "SUPPRIMER", "LÖSCHEN", "ELIMINA", "ELIMINAR", "УДАЛИТЬ ИГРУ", "APAGAR JOGO", "EXCLUIR JOGO"} },

        { "UI_TAB_CONTROLS", {"CONTROLS", "COMMANDES", "STEUERUNG", "COMANDI", "CONTROLES", "УПРАВЛЕНИЕ", "COMANDOS", "CONTROLES"} },
        { "UI_TAB_GAME",     {"GAME", "JEU", "SPIEL", "GIOCO", "JUEGO", "ИГРА", "JOGO", "JOGO"} },
        { "UI_TAB_GRAPHICS", {"DISPLAY", "AFFICHAGE", "GRAFIK", "GRAFICA", "PANTALLA", "ГРАФИКА", "ECRÃ", "TELA"} },
        { "UI_TAB_SOUND",    {"AUDIO", "AUDIO", "AUDIO", "AUDIO", "AUDIO", "ЗВУК", "ÁUDIO", "ÁUDIO"} },
        { "UI_TAB_OPTIONS",  {"OPTIONS", "OPTIONS", "OPTIONEN", "OPZIONI", "OPCIONES", "ОПЦИИ", "OPÇÕES", "OPÇÕES"} },

        { "UI_BACK",   {"BACK", "RETOUR", "ZURÜCK", "INDIETRO", "ATRÁS", "НАЗАД", "VOLTAR", "VOLTAR"} },
        { "UI_RESET",  {"RESET", "RÉINIT.", "RESET", "RESET", "RESTABLECER", "СБРОС", "REPOR", "REDEFINIR"} },
        { "UI_REBIND_WAIT", {"Waiting for input...", "En attente d'une touche...", "Warte auf Eingabe...", "In attesa di un tasto...", "Esperando una tecla...", "Ожидание ввода...", "À espera de input...", "Aguardando entrada..."} },
        { "UI_REBIND_ESC",  {"Press [esc] to cancel", "Appuie sur [esc] pour annuler", "Zum Abbrechen [esc] drücken", "Premi [esc] per annullare", "Pulsa [esc] para cancelar", "Для отмены нажмите [esc]", "Prima [esc] para cancelar", "Pressione [esc] para cancelar"} },
        { "UI_KEY_SPACE",   {"SPACE", "ESPACE", "LEERTASTE", "SPAZIO", "ESPACIO", "ПРОБЕЛ", "ESPAÇO", "ESPAÇO"} },
        { "UI_KEY_LSHIFT",  {"L. SHIFT", "MAJ. G", "L. UMSCHALT", "SHIFT S", "MAYÚS. I", "Л. SHIFT", "SHIFT ESQ.", "SHIFT ESQ."} },
        { "UI_KEY_RSHIFT",  {"R. SHIFT", "MAJ. D", "R. UMSCHALT", "SHIFT D", "MAYÚS. D", "П. SHIFT", "SHIFT DIR.", "SHIFT DIR."} },
        { "UI_KEY_LCTRL",   {"L. CTRL", "CTRL G", "L. STRG", "CTRL S", "CTRL I", "Л. CTRL", "CTRL ESQ.", "CTRL ESQ."} },
        { "UI_KEY_RCTRL",   {"R. CTRL", "CTRL D", "R. STRG", "CTRL D", "CTRL D", "П. CTRL", "CTRL DIR.", "CTRL DIR."} },
        { "UI_KEY_LALT",    {"L. ALT", "ALT G", "L. ALT", "ALT S", "ALT I", "Л. ALT", "ALT ESQ.", "ALT ESQ."} },
        { "UI_KEY_RALT",    {"R. ALT", "ALT D", "R. ALT", "ALT D", "ALT D", "П. ALT", "ALT DIR.", "ALT DIR."} },
        { "UI_KEY_MOUSE4",  {"Mouse 4", "Souris 4", "Maus 4", "Mouse 4", "Ratón 4", "Мышь 4", "Rato 4", "Mouse 4"} },
        { "UI_KEY_MOUSE5",  {"Mouse 5", "Souris 5", "Maus 5", "Mouse 5", "Ratón 5", "Мышь 5", "Rato 5", "Mouse 5"} },
        { "UI_CANCEL", {"Cancel", "Annuler", "Abbrechen", "Annulla", "Cancelar", "Отмена", "Cancelar", "Cancelar"} },
        { "UI_CONFIRM",{"Confirm", "Confirmer", "Bestätigen", "Conferma", "Confirmar", "Подтвердить", "Confirmar", "Confirmar"} },
        { "UI_OK",     { "OK", "OK", "OK", "OK", "OK", "OK", "OK", "OK" } },
        { "UI_ARE_YOU_SURE", {"Are you sure?", "Êtes-vous sûr ?", "Sind Sie sicher?", "Sei sicuro?", "¿Estás seguro?", "Вы уверены?", "Tem a certeza?", "Tem certeza?"} },
        { "UI_NEW_GAME_PROMPT", {"Are you sure you want to start a new game?", "Voulez-vous vraiment commencer une nouvelle partie ?", "Wirklich ein neues Spiel beginnen?", "Vuoi davvero iniziare una nuova partita?", "¿Seguro que quieres comenzar una nueva partida?", "Вы действительно хотите начать новую игру?", "Tem a certeza de que quer começar um novo jogo?", "Tem certeza de que deseja começar um novo jogo?"} },
        { "UI_LOAD_PROMPT", {"Do you want to load the save \"%s\"?", "Voulez-vous charger la sauvegarde « %s » ?", "Möchten Sie den Spielstand „%s“ laden?", "Vuoi caricare il salvataggio '%s'?", "¿Quieres cargar la partida \"%s\"?", "Вы хотите загрузить сохранение '%s'?", "Quer carregar o save \"%s\"?", "Deseja carregar o save \"%s\"?"} },
        { "UI_DELETE_PROMPT", {"Do you want to delete the save \"%s\"?", "Voulez-vous supprimer la sauvegarde « %s » ?", "Möchten Sie den Spielstand „%s“ löschen?", "Vuoi eliminare il salvataggio '%s'?", "¿Quieres eliminar la partida \"%s\"?", "Вы хотите удалить сохранение '%s'?", "Quer apagar o save \"%s\"?", "Deseja excluir o save \"%s\"?"} },
        { "UI_OVERWRITE_PROMPT", {"Do you want to overwrite the save \"%s\"?", "Voulez-vous écraser la sauvegarde « %s » ?", "Möchten Sie den Spielstand „%s“ überschreiben?", "Vuoi sovrascrivere il salvataggio '%s'?", "¿Quieres sobrescribir la partida \"%s\"?", "Вы хотите перезаписать сохранение \"%s\"?", "Quer substituir o save \"%s\"?", "Deseja sobrescrever o save \"%s\"?"} },
        { "UI_SAVE_SUCCESS", {"Save successful.\nPress OK to continue.", "Sauvegarde réussie.\nAppuyez sur OK pour continuer.", "Speichern erfolgreich.\nDrücken Sie OK, um fortzufahren.", "Salvataggio riuscito.\nPremi OK per continuare.", "Partida guardada.\nPulsa OK para continuar.", "Сохранение прошло успешно.\nНажмите OK, чтобы продолжить.", "Guardado com sucesso.\\nPrima OK para continuar.", "Salvo com sucesso.\\nPressione OK para continuar."} },

        { "UI_EMPTY_SLOT",  {"Empty slot", "Emplacement vide", "Leerer Slot", "Slot vuoto", "Ranura vacía", "Пустая ячейка", "Vazio", "Vazio"} },
        { "UI_CORRUPT_SAVE",{"Damaged file", "Fichier endommagé", "Beschädigte Datei", "File danneggiato", "Archivo dañado", "Повреждённый файл", "Corrupto", "Corrompido"} },
        { "MON_01", { "JAN", "JAN", "JAN", "GEN", "ENE", "ЯНВ", "ENE", "ENE" } },
        { "MON_02", { "FEB", "FÉV", "FEB", "FEB", "FEB", "ФЕВ", "FEB", "FEB" } },
        { "MON_03", { "MAR", "MAR", "MÄR", "MAR", "MAR", "МАР", "MAR", "MAR" } },
        { "MON_04", { "APR", "AVR", "APR", "APR", "ABR", "АПР", "ABR", "ABR" } },
        { "MON_05", { "MAY", "MAI", "MAI", "MAG", "MAY", "МАЙ", "MAY", "MAY" } },
        { "MON_06", { "JUN", "JUIN", "JUN", "GIU", "JUN", "ИЮН", "JUN", "JUN" } },
        { "MON_07", { "JUL", "JUIL", "JUL", "LUG", "JUL", "ИЮЛ", "JUL", "JUL" } },
        { "MON_08", { "AUG", "AOÛT", "AUG", "AGO", "AGO", "АВГ", "AGO", "AGO" } },
        { "MON_09", { "SEP", "SEP", "SEP", "SET", "SEP", "СЕН", "SEP", "SEP" } },
        { "MON_10", { "OCT", "OCT", "OKT", "OTT", "OCT", "ОКТ", "OCT", "OCT" } },
        { "MON_11", { "NOV", "NOV", "NOV", "NOV", "NOV", "НОЯ", "NOV", "NOV" } },
        { "MON_12", { "DEC", "DÉC", "DEZ", "DIC", "DIC", "ДЕК", "DIC", "DIC" } },

        { "MAP_HINTS", {
            "Zoom [SCROLL]    Place marker [LMB]    Remove marker [RMB]    Back [ESC]    Legend [TAB]",
            "Zoom [SCROLL]    Placer un marqueur [LMB]    Retirer le marqueur [RMB]    Retour [ESC]    Légende [TAB]",
            "Zoom [SCROLL]    Markierung setzen [LMB]    Markierung entfernen [RMB]    Zurück [ESC]    Legende [TAB]",
            "Zoom [SCROLL]    Posiziona marcatore [LMB]    Rimuovi marcatore [RMB]    Indietro [ESC]    Legenda [TAB]",
            "Zoom [SCROLL]    Colocar marca [LMB]    Quitar marca [RMB]    Atrás [ESC]    Leyenda [TAB]",
            "Масштаб [СКРОЛЛ]    Поставить метку [ЛКМ]    Убрать метку [ПКМ]    Назад [ESC]    Легенда [TAB]",
            "Zoom [SCROLL]    Colocar marcador [LMB]    Remover marcador [RMB]    Voltar [ESC]    Legenda [TAB]",
            "Zoom [SCROLL]    Colocar marcador [LMB]    Remover marcador [RMB]    Voltar [ESC]    Legenda [TAB]"
        } },
        { "ZONE_DEFAULT", { "SAN ANDREAS", "SAN ANDREAS", "SAN ANDREAS", "SAN ANDREAS", "SAN ANDREAS", "САН АНДРЕАС", "SAN ANDREAS", "SAN ANDREAS" } },

        { "SET_REDEFINE",     { "Redefine Controls", "Redéfinir les commandes", "Steuerung ändern", "Ridefinisci comandi", "Cambiar controles", "Изменить раскладку управления", "Cambiar controles", "Cambiar controles" } },
        { "SET_INVERT_Y",     { "Invert Look", "Inverser le regard", "Blick umkehren", "Inverti visuale", "Invertir mirada", "Инвертировать обзор", "Invertir mirada", "Invertir mirada" } },
        { "SET_INVERT_X",     { "Invert Look Horizontal", "Inverser regard horizontal", "Blick horizontal umkehren", "Inverti visuale orizz.", "Invertir mirada horizontal", "Инвертировать обзор по горизонтали", "Invertir mirada horizontal", "Invertir mirada horizontal" } },
        { "SET_MOUSE_X",      { "Mouse X Sensitivity", "Sensibilité souris X", "Mausempfindlichkeit X", "Sensibilità mouse X", "Sensibilidad ratón X", "Чувствительность оси X", "Sensibilidad ratón X", "Sensibilidad ratón X" } },
        { "SET_MOUSE_Y",      { "Mouse Y Sensitivity", "Sensibilité souris Y", "Mausempfindlichkeit Y", "Sensibilità mouse Y", "Sensibilidad ratón Y", "Чувствительность оси Y", "Sensibilidad ratón Y", "Sensibilidad ratón Y" } },
        { "SET_MOUSE_STEER",  { "Mouse Steering", "Conduite à la souris", "Mauslenkung", "Sterzo mouse", "Dirección con ratón", "Руление мышью", "Dirección con ratón", "Dirección con ratón" } },
        { "SET_MOUSE_FLY",    { "Mouse Flying", "Vol à la souris", "Mausflug", "Volo mouse", "Vuelo con ratón", "Полёт мышью", "Vuelo con ratón", "Vuelo con ratón" } },
        { "SET_RESTORE",      { "Restore Defaults", "Restaurer", "Standard wiederherstellen", "Ripristina predefiniti", "Restaurar valores", "Восстановить по умолчанию", "Restaurar valores", "Restaurar valores" } },
        { "SET_SHOW_HUD",     { "Show HUD", "Afficher le HUD", "HUD anzeigen", "Mostra HUD", "Mostrar HUD", "Показывать HUD", "Mostrar HUD", "Mostrar HUD" } },
        { "SET_RADAR",        { "Radar Mode", "Mode radar", "Radarmodus", "Modalità radar", "Modo radar", "Режим радара", "Modo radar", "Modo radar" } },
        { "SET_WIDESCREEN",   { "Widescreen", "Grand écran", "Breitbild", "Widescreen", "Pantalla ancha", "Широкий экран", "Pantalla ancha", "Pantalla ancha" } },
        { "SET_BRIGHTNESS",   { "Brightness", "Luminosité", "Helligkeit", "Luminosità", "Brillo", "Яркость", "Brillo", "Brillo" } },
        { "SET_DRAW_DIST",    { "Draw Distance", "Distance d'affichage", "Zeichnungsweite", "Distanza di disegno", "Distancia de dibujo", "Дальность прорисовки", "Distancia de dibujo", "Distancia de dibujo" } },
        { "SET_FRAME_LIMIT",  { "Frame Limiter", "Limiteur d'images", "Bildratenbegrenzer", "Limite fotogrammi", "Limitador de fotogramas", "Ограничение частоты кадров", "Limitador de fotogramas", "Limitador de fotogramas" } },
        { "SET_FX_QUALITY",   { "Effects Quality", "Qualité des effets", "Effektqualität", "Qualità effetti", "Calidad de efectos", "Качество эффектов", "Calidad de efectos", "Calidad de efectos" } },
        { "SET_MIP",          { "Mip Mapping", "Mip Mapping", "Mip Mapping", "Mip Mapping", "Mip Mapping", "Mip Mapping", "Mip Mapping", "Mip Mapping" } },
        { "SET_AA",           { "Anti-Aliasing", "Anticrénelage", "Antialiasing", "Anti-aliasing", "Suavizado", "Сглаживание", "Suavizado", "Suavizado" } },
        { "SET_WINDOW_MODE",  { "Window Mode", "Mode fenêtre", "Fenstermodus", "Modalità finestra", "Modo de ventana", "Режим окна", "Modo de ventana", "Modo de ventana" } },
        { "SET_RESOLUTION",   {"Resolution", "Résolution", "Auflösung", "Risoluzione", "Resolución", "Разрешение", "Resolução", "Resolução"} },
        { "SET_HEATHAZE",     {"Heat Haze", "Effet de chaleur", "Hitzeflimmern", "Foschia di calore", "Efecto de calor", "Эффект жары", "Efeito de calor", "Efeito de calor"} },
        { "SET_SPEEDBLUR",    {"Speed Blur", "Flou de vitesse", "Geschwindigkeitsunschärfe", "Sfocatura velocità", "Desenfoque de velocidad", "Размытие на скорости", "Desfoque de velocidade", "Desfoque de velocidade"} },
        { "SET_SFX",          {"SFX Volume", "Volume SFX", "SFX-Lautstärke", "Volume SFX", "Volumen SFX", "Громкость звуков", "Volume SFX", "Volume SFX"} },
        { "SET_MUSIC",        {"Music Volume", "Volume musique", "Musiklautstärke", "Volume musica", "Volumen música", "Громкость музыки", "Volume da música", "Volume da música"} },
        { "SET_RADIO_AUTO",   {"Radio Autotune", "Radio auto", "Radio Auto", "Radio auto", "Radio auto", "Автовыбор радиостанции", "Rádio automática", "Rádio automática"} },
        { "SET_RADIO_EQ",     {"Radio EQ", "Égaliseur radio", "Radio-EQ", "EQ radio", "EQ de radio", "Эквалайзер радио", "EQ do rádio", "EQ do rádio"} },
        { "SET_LANGUAGE",     {"Language", "Langue", "Sprache", "Lingua", "Idioma", "Язык", "Idioma", "Idioma"} },
        { "SET_SUBTITLES",    {"Subtitles", "Sous-titres", "Untertitel", "Sottotitoli", "Subtítulos", "Субтитры", "Legendas", "Legendas"} },
        { "SET_UPDATED_HELP", {"Updated Help", "Aide actualisée", "Aktualisierte Hinweise", "Suggerimenti aggiornati", "Consejos actualizados", "Обновленные подсказки", "Ajuda atualizada", "Ajuda atualizada"} },
        { "SET_RADIO_TEXT",   {"Radio Name HUD", "Nom radio HUD", "Radio-Name HUD", "Nome radio HUD", "Nombre de radio HUD", "Название радио HUD", "Nome do rádio no HUD", "Nome do rádio no HUD"} },
        { "SET_GPS",          {"GPS Route", "Itinéraire GPS", "GPS-Route", "Percorso GPS", "Ruta GPS", "Маршрут GPS", "Rota GPS", "Rota GPS"} },
        { "SET_PHOTOS",       {"Store Photos", "Stocker photos", "Fotos speichern", "Salva foto", "Guardar fotos", "Сохранять фотографии", "Guardar fotos", "Salvar fotos"} },
        { "SET_DE_ICONS",     {"Definitive Icons", "Icônes Definitive", "Definitive-Symbole", "Icone Definitive", "Iconos Definitive", "Иконки Definitive", "Ícones Definitive", "Ícones Definitive"} },
        { "SET_CUSTOM_RADAR", {"Custom Radar Map", "Carte radar perso", "Eigene Radarkarte", "Mappa radar personalizzata", "Mapa radar personal", "Кастомная карта радара", "Mapa de radar personalizado", "Mapa de radar personalizado"} },
        { "SET_GANG_ZONES",   {"Gang Zones", "Zones de gangs", "Ganggebiete", "Zone gang", "Zonas de bandas", "Зоны банд", "Zonas de gangs", "Zonas de gangs"} },

        { "RADAR_MAP_BLIPS", {"Map & Blips", "Carte et repères", "Karte & Markierungen", "Mappa e icone", "Mapa y marcas", "Карта и метки", "Mapa e marcas", "Mapa e marcas"} },
        { "RADAR_BLIPS",     {"Blips only", "Repères seuls", "Nur Markierungen", "Solo icone", "Solo marcas", "Только метки", "Só marcas", "Só marcas"} },
        { "RADAR_OFF",       {"Off", "Désactivé", "Aus", "No", "No", "Выкл.", "Desligado", "Desligado"} },

        { "WIN_WINDOWED",    {"Windowed", "Fenêtré", "Fenster", "Finestra", "Ventana", "Оконный", "Janela", "Janela"} },
        { "WIN_FULLSCREEN",  {"Fullscreen", "Plein écran", "Vollbild", "Schermo intero", "Pantalla completa", "Полноэкранный", "Ecrã inteiro", "Tela cheia"} },
        { "WIN_BORDERLESS",  {"Windowed (Borderless)", "Fenêtré (sans bord)", "Fenster (rahmenlos)", "Finestra (senza bordo)", "Ventana (sin marco)", "Оконный (без рамки)", "Janela (sem moldura)", "Janela (sem borda)"} },

        // Radio HUD — GXT keys FEA_R* / FEA_MP3 / FEA_NON. Brands stay EN; no 1C/SanLtd tags.
        { "FEA_R0",  { "Playback FM", "Playback FM", "Playback FM", "Playback FM", "Playback FM", "Playback FM", "Playback FM", "Playback FM" } },
        { "FEA_R1",  { "K-Rose", "K-Rose", "K-Rose", "K-Rose", "K-Rose", "K-Rose", "K-Rose", "K-Rose" } },
        { "FEA_R2",  { "K-DST", "K-DST", "K-DST", "K-DST", "K-DST", "K-DST", "K-DST", "K-DST" } },
        { "FEA_R3",  { "Bounce FM", "Bounce FM", "Bounce FM", "Bounce FM", "Bounce FM", "Bounce FM", "Bounce FM", "Bounce FM" } },
        { "FEA_R4",  { "SF-UR", "SF-UR", "SF-UR", "SF-UR", "SF-UR", "SF-UR", "SF-UR", "SF-UR" } },
        { "FEA_R5",  { "Radio Los Santos", "Radio Los Santos", "Radio Los Santos", "Radio Los Santos", "Radio Los Santos", "Радио Лос-Сантос", "Radio Los Santos", "Radio Los Santos" } },
        { "FEA_R6",  { "Radio X", "Radio X", "Radio X", "Radio X", "Radio X", "Radio X", "Radio X", "Radio X" } },
        { "FEA_R7",  { "CSR 103.9", "CSR 103.9", "CSR 103.9", "CSR 103.9", "CSR 103.9", "CSR 103.9", "CSR 103.9", "CSR 103.9" } },
        { "FEA_R8",  { "K-Jah West", "K-Jah West", "K-Jah West", "K-Jah West", "K-Jah West", "K-Jah West", "K-Jah West", "K-Jah West" } },
        { "FEA_R9",  { "Master Sounds 98.3", "Master Sounds 98.3", "Master Sounds 98.3", "Master Sounds 98.3", "Master Sounds 98.3", "Master Sounds 98.3", "Master Sounds 98.3", "Master Sounds 98.3" } },
        { "FEA_R10", { "WCTR", "WCTR", "WCTR", "WCTR", "WCTR", "WCTR", "WCTR", "WCTR" } },
        { "FEA_MP3", {"User Track Player", "Lecteur de pistes perso", "User-Track-Player", "Lettore tracce utente", "Reproductor de pistas", "Пользовательские треки", "Leitor de faixas do utilizador", "Player de faixas do usuário"} },
        { "FEA_NON", {"Radio Off", "Radio éteinte", "Radio aus", "Radio spenta", "Radio apagada", "Радио выключено", "Rádio desligado", "Rádio desligado"} },

        { "FX_LOW",    {"Low", "Faible", "Niedrig", "Bassa", "Baja", "Низк.", "Baixo", "Baixo"} },
        { "FX_MED",    {"Medium", "Moyen", "Mittel", "Media", "Media", "Средн.", "Médio", "Médio"} },
        { "FX_HIGH",   {"High", "Élevé", "Hoch", "Alta", "Alta", "Высок.", "Alto", "Alto"} },
        { "FX_VERY",   {"Very High", "Très élevé", "Sehr hoch", "Molto alta", "Muy alta", "Очень высок.", "Muito alto", "Muito alto"} },
    };

    const Loc8 kZones[] = {
#include "ZoneDict.inc"
    };

    const char* LookupLoc(const Loc8* rows, size_t count, const char* key, LanguageManager::Lang lang)
    {
        if (!key || !key[0])
            return nullptr;
        const int li = static_cast<int>(lang);
        if (li < 0 || li >= static_cast<int>(LanguageManager::Lang::Count))
            return nullptr;
        for (size_t i = 0; i < count; ++i)
        {
            if (std::strcmp(rows[i].key, key) == 0)
            {
                const char* s = rows[i].t[li];
                if (s && s[0])
                    return s;
                s = rows[i].t[static_cast<int>(LanguageManager::Lang::American)];
                return (s && s[0]) ? s : rows[i].key;
            }
        }
        return nullptr;
    }

    void NormZoneKey(const char* in, char* out, size_t outChars)
    {
        if (!out || outChars == 0)
            return;
        out[0] = 0;
        if (!in)
            return;
        size_t n = 0;
        for (; in[n] && n + 1 < outChars && n < 8; ++n)
        {
            const unsigned char c = static_cast<unsigned char>(in[n]);
            if (c == 0 || c <= ' ')
                break;
            out[n] = static_cast<char>(std::toupper(c));
        }
        out[n] = 0;
    }

    const char* LookupZoneKey(const char* raw)
    {
        char key[16]{};
        NormZoneKey(raw, key, sizeof(key));
        if (!key[0])
            return nullptr;

        const size_t nZones = sizeof(kZones) / sizeof(kZones[0]);
        if (const char* s = LookupLoc(kZones, nZones, key, g_lang))
            return s;

        // LIND1a / SFDWT6 / WESTP2 → base GXT key
        int n = static_cast<int>(std::strlen(key));
        while (n > 2)
        {
            const unsigned char c = static_cast<unsigned char>(key[n - 1]);
            if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'C' && n > 3
                                           && key[n - 2] >= '0' && key[n - 2] <= '9'))
            {
                key[--n] = 0;
                if (const char* s = LookupLoc(kZones, nZones, key, g_lang))
                    return s;
                continue;
            }
            break;
        }
        return nullptr;
    }

    const char* LangKey(LanguageManager::Lang lang)
    {
        switch (lang)
        {
        case LanguageManager::Lang::American: return "American";
        case LanguageManager::Lang::French:   return "French";
        case LanguageManager::Lang::German:   return "German";
        case LanguageManager::Lang::Italian:  return "Italian";
        case LanguageManager::Lang::Spanish:  return "Spanish";
        case LanguageManager::Lang::Russian:     return "Russian";
        case LanguageManager::Lang::Portuguese:  return "Portuguese";
        case LanguageManager::Lang::Brazilian:   return "Brazilian";
        default:                                 return "Russian";
        }
    }

    LanguageManager::Lang ParseLang(const char* s)
    {
        if (!s || !s[0])
            return LanguageManager::Lang::Russian;
        if (_stricmp(s, "American") == 0 || _stricmp(s, "English") == 0)
            return LanguageManager::Lang::American;
        if (_stricmp(s, "French") == 0)
            return LanguageManager::Lang::French;
        if (_stricmp(s, "German") == 0)
            return LanguageManager::Lang::German;
        if (_stricmp(s, "Italian") == 0)
            return LanguageManager::Lang::Italian;
        if (_stricmp(s, "Spanish") == 0)
            return LanguageManager::Lang::Spanish;
        if (_stricmp(s, "Russian") == 0)
            return LanguageManager::Lang::Russian;
        if (_stricmp(s, "Portuguese") == 0 || _stricmp(s, "PortuguesePT") == 0 || _stricmp(s, "pt") == 0)
            return LanguageManager::Lang::Portuguese;
        if (_stricmp(s, "Brazilian") == 0 || _stricmp(s, "PortugueseBR") == 0
            || _stricmp(s, "pt-BR") == 0 || _stricmp(s, "pt_BR") == 0)
            return LanguageManager::Lang::Brazilian;
        return LanguageManager::Lang::Russian;
    }
}

LanguageManager::Lang LanguageManager::GetLanguage()
{
    return g_lang;
}

void LanguageManager::ApplyGameLanguage(Lang lang)
{
    // Stock SA GXT slots: 0..4 (EN/FR/DE/IT/ES). Russian UI uses slot 0 (RU packs replace american.gxt).
    char game = 0;
    switch (lang)
    {
    case Lang::American:
    case Lang::Russian:
    case Lang::Portuguese:
    case Lang::Brazilian: game = 0; break; // stock SA has no PT GXT slot
    case Lang::French:    game = 1; break;
    case Lang::German:    game = 2; break;
    case Lang::Italian:   game = 3; break;
    case Lang::Spanish:   game = 4; break;
    default:              game = 0; break;
    }
    if (FrontEndMenuManager.m_nPrefsLanguage != game)
    {
        FrontEndMenuManager.m_nPrefsLanguage = game;
        FrontEndMenuManager.InitialiseChangedLanguageSettings(true);
    }
}

void LanguageManager::SetLanguage(Lang lang, bool persist)
{
    if (lang < Lang::American || lang >= Lang::Count)
        lang = Lang::American;
    g_lang = lang;
    ApplyGameLanguage(lang);
    if (persist)
        RadarConfig::SetUiLanguage(LangKey(lang));
}

void LanguageManager::ApplySavedLanguage()
{
    SetLanguage(ParseLang(RadarConfig::GetUiLanguage()), false);
}

void LanguageManager::CycleLanguage(int dir)
{
    int n = static_cast<int>(g_lang) + (dir >= 0 ? 1 : -1);
    const int count = static_cast<int>(Lang::Count);
    if (n < 0) n = count - 1;
    if (n >= count) n = 0;
    SetLanguage(static_cast<Lang>(n));
}

const char* LanguageManager::Get(const char* key)
{
    if (const char* s = LookupLoc(kUi, sizeof(kUi) / sizeof(kUi[0]), key, g_lang))
        return s;
    if (const char* s = HelpGxt::Get(key))
        return s;
    return key ? key : "";
}

const char* LanguageManager::GetRadioStation(int stationId)
{
    const char* key = nullptr;
    switch (stationId)
    {
    case 1:  key = "FEA_R0";  break;
    case 2:  key = "FEA_R1";  break;
    case 3:  key = "FEA_R2";  break;
    case 4:  key = "FEA_R3";  break;
    case 5:  key = "FEA_R4";  break;
    case 6:  key = "FEA_R5";  break;
    case 7:  key = "FEA_R6";  break;
    case 8:  key = "FEA_R7";  break;
    case 9:  key = "FEA_R8";  break;
    case 10: key = "FEA_R9";  break;
    case 11: key = "FEA_R10"; break;
    case 12: key = "FEA_MP3"; break;
    case 13: key = "FEA_NON"; break;
    default: return "";
    }
    return Get(key);
}

const char* LanguageManager::GetZone(const char* zoneKey)
{
    if (const char* s = LookupZoneKey(zoneKey))
        return s;
    return Get("ZONE_DEFAULT");
}

const char* LanguageManager::LookupZone(const char* zoneKey)
{
    return LookupZoneKey(zoneKey);
}

const char* LanguageManager::GetLanguageName(Lang lang)
{
    switch (lang)
    {
    case Lang::American: return Get("LANG_AMERICAN");
    case Lang::French:   return Get("LANG_FRENCH");
    case Lang::German:   return Get("LANG_GERMAN");
    case Lang::Italian:  return Get("LANG_ITALIAN");
    case Lang::Spanish:     return Get("LANG_SPANISH");
    case Lang::Russian:     return Get("LANG_RUSSIAN");
    case Lang::Portuguese:  return Get("LANG_PORTUGUESE");
    case Lang::Brazilian:   return Get("LANG_BRAZILIAN");
    default:                return Get("LANG_AMERICAN");
    }
}
