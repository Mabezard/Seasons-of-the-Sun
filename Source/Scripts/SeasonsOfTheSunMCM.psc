Scriptname SeasonsOfTheSunMCM extends SKI_ConfigBase

; ==============================================================================
; SCRIPT PROPERTIES & CONSTANTS
; ==============================================================================

; --- Preset Labels (SkyUI Safe) ---
String[] PROPERTY PresetNames Auto

; --- MCM Option IDs ---
int oid_preset_list
int oid_latitude_slider
int oid_toggle_faster_seasons

; --- Configuration State ---
bool bInitialized = false
int PROPERTY CurrentPresetIndex = 0 Auto Hidden
float PROPERTY CurrentLatitude = -53.0 Auto Hidden
bool PROPERTY IsFasterSeasons = false Auto Hidden

; --- Reference to Quest holding seasonal logic ---
SeasonsOfTheSunQuest PROPERTY QuestRef Auto


; ==============================================================================
; TOOLTIP STRINGS (ASCII Safe - Standard Properties)
; ==============================================================================

String PROPERTY ST_TOOLTIP_PRESET_LIST = "Preset Location:\nSelect a latitude" Auto

String PROPERTY ST_TOOLTIP_LATITUDE_SLIDER = "Latitude Setting:\nCustom Latitude. Negative values for Northern Hemisphere positive values for Southern Hemisphere" Auto

String PROPERTY ST_TOOLTIP_PRESET_NO_EUROPE = "Northern Europe (N 53 deg):\nSimilar to Hamburg Germany. Summer peak: -29.6 deg, Winter trough: -76.4 deg. Ideal for authentic Nordic sky trajectories. Southern mountains cast long shadows in the winter" Auto

String PROPERTY ST_TOOLTIP_PRESET_US = "United States (N 39 deg):\nSimilar to Denver Colorado. Summer peak: -15.6 deg, Winter trough: -62.4 deg. Provides higher solar elevation year-round." Auto

String PROPERTY ST_TOOLTIP_PRESET_AUSTRALIA = "Australia (S 27 deg):\nSimilar to Brisbane Australia. Sun tilts toward North horizon. Provides higher solar elevation for less shadows for a warmer brighter climate." Auto

String PROPERTY ST_TOOLTIP_PRESET_NEW_ZEALAND = "New Zealand (S 41 deg):\nSimilar to Wellington New Zealand. Sun tilts toward North horizon. Summer peak: +64.4 deg, Winter trough: +17.6 deg. Ideal for a LOTR fantasy, Less shadows cast from the Northern Sea" Auto

String PROPERTY ST_TOOLTIP_FASTER_SEASONS = "Calendar System Toggle:\n• OFF (365 Days): Standard year. Summer Solstice on Midyear 21st; New Game (Last Seed 17th) sits 56 days post- summer solstice.\n• ON (120 Days - Faster Seasons): 3-cycle compressed year. For compatibility with Faster Seasons of Skyrim." Auto


; ==============================================================================
; INITIALIZATION & MCM SETUP
; ==============================================================================

Event OnConfigInit()
    ModName = "Seasons of the Sun"
    
    PresetNames = new String[4]
    PresetNames[0] = "N Europe (N 53 deg)"
    PresetNames[1] = "USA (N 39 deg)"
    PresetNames[2] = "Australia (S 27 deg)"
    PresetNames[3] = "New Zealand (S 41 deg)"

    ; Resolve Quest Reference dynamically if unbound
    if (QuestRef == None)
        QuestRef = Quest.GetQuest("SeasonsOfTheSun_Quest") as SeasonsOfTheSunQuest
    endIf
EndEvent

Event OnPageReset(String a_page)
    SetCursorFillMode(TOP_TO_BOTTOM)
    
    ; --- Left Column: Geographic Presets & Latitude ---
    AddHeaderOption("Geographic Location")
    oid_preset_list = AddMenuOption("Location Preset", PresetNames[CurrentPresetIndex])
    oid_latitude_slider = AddSliderOption("Custom Latitude", CurrentLatitude, "{1} deg")
    
    AddEmptyOption()
    
    ; --- Right Column: Calendar Controls ---
    SetCursorPosition(1)
    AddHeaderOption("Calendar Controls")
    oid_toggle_faster_seasons = AddToggleOption("Faster Seasons (120 Days)", IsFasterSeasons)
EndEvent


; ==============================================================================
; HIGHLIGHT EVENT HANDLER
; ==============================================================================

Event OnOptionHighlight(int a_option)
    if (a_option == oid_preset_list)
        if (CurrentPresetIndex == 0)
            SetInfoText(ST_TOOLTIP_PRESET_NO_EUROPE)
        elseIf (CurrentPresetIndex == 1)
            SetInfoText(ST_TOOLTIP_PRESET_US)
        elseIf (CurrentPresetIndex == 2)
            SetInfoText(ST_TOOLTIP_PRESET_AUSTRALIA)
        elseIf (CurrentPresetIndex == 3)
            SetInfoText(ST_TOOLTIP_PRESET_NEW_ZEALAND)
        else
            SetInfoText(ST_TOOLTIP_PRESET_LIST)
        endIf
    elseIf (a_option == oid_latitude_slider)
        SetInfoText(ST_TOOLTIP_LATITUDE_SLIDER)
    elseIf (a_option == oid_toggle_faster_seasons)
        SetInfoText(ST_TOOLTIP_FASTER_SEASONS)
    endIf
EndEvent


; ==============================================================================
; INTERACTION EVENT HANDLERS
; ==============================================================================

; --- Menu Selection ---
Event OnOptionMenuOpen(int a_option)
    if (a_option == oid_preset_list)
        SetMenuDialogOptions(PresetNames)
        SetMenuDialogStartIndex(CurrentPresetIndex)
        SetMenuDialogDefaultIndex(0)
    endIf
EndEvent

Event OnOptionMenuAccept(int a_option, int a_index)
    if (a_option == oid_preset_list)
        CurrentPresetIndex = a_index
        SetMenuOptionValue(oid_preset_list, PresetNames[CurrentPresetIndex])
        
        ; Apply preset configurations
        if (CurrentPresetIndex == 0) ; Northern Europe
            CurrentLatitude = -53.0
        elseIf (CurrentPresetIndex == 1) ; United States
            CurrentLatitude = -39.0
        elseIf (CurrentPresetIndex == 2) ; Australia
            CurrentLatitude = 27.0
        elseIf (CurrentPresetIndex == 3) ; New Zealand
            CurrentLatitude = 41.0
        endIf
        
        ForcePageReset()
        RecalculateAndApply()
    endIf
EndEvent

; --- Slider Selection ---
Event OnOptionSliderOpen(int a_option)
    if (a_option == oid_latitude_slider)
        SetSliderDialogStartValue(CurrentLatitude)
        SetSliderDialogDefaultValue(-53.0)
        SetSliderDialogRange(-65.0, 65.0)
        SetSliderDialogInterval(1.0)
    endIf
EndEvent

Event OnOptionSliderAccept(int a_option, float a_value)
    if (a_option == oid_latitude_slider)
        CurrentLatitude = a_value
        SetSliderOptionValue(oid_latitude_slider, CurrentLatitude, "{1} deg")
        RecalculateAndApply()
    endIf
EndEvent

; --- Toggle Selection ---
Event OnOptionSelect(int a_option)
    if (a_option == oid_toggle_faster_seasons)
        IsFasterSeasons = !IsFasterSeasons
        SetToggleOptionValue(oid_toggle_faster_seasons, IsFasterSeasons)
        RecalculateAndApply()
    endIf
EndEvent


; ==============================================================================
; RECALCULATION ROUTINE
; ==============================================================================

Function RecalculateAndApply()
    ; Double check quest pointer fallback
    if (QuestRef == None)
        QuestRef = Quest.GetQuest("SeasonsOfTheSun_Quest") as SeasonsOfTheSunQuest
    endIf

    ; 1. Sync properties onto the Quest script instance
    if (QuestRef != None)
        QuestRef.BaseLatitude = CurrentLatitude
        QuestRef.UseFasterSeasons = IsFasterSeasons
        QuestRef.EvaluateAngle()
    else
        ; 2. Fallback Direct Execution if Quest is still unassigned
        Float currentGameTime = Utility.GetCurrentGameTime()
        Float totalDays = 365.0
        if (IsFasterSeasons)
            totalDays = 120.0
        endif

        Float rawDay = currentGameTime - ((currentGameTime / totalDays) as Int * totalDays)
        Int dayOfYear = rawDay as Int
        Int totalDaysInt = totalDays as Int
        Bool invertSouthern = (CurrentLatitude > 0.0)

        Float calculatedAngle = SeasonsOfTheSunNative.GetCalculatedSunAngle(CurrentLatitude, dayOfYear, totalDaysInt, invertSouthern)
        Debug.Notification("Seasonal Sun Angle: MCM Updated -> " + calculatedAngle + " deg")
    endIf
EndFunction