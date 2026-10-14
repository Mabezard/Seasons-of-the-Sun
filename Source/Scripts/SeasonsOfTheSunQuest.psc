ScriptName SeasonsOfTheSunQuest Extends Quest


; ---------------------------------------------------------------------------
; PROPERTIES
; ---------------------------------------------------------------------------

Float Property BaseLatitude = -53.0 Auto ; N Europe
Bool Property UseFasterSeasons = False Auto ; Default: Full year (365 days)

GlobalVariable Property Seasonal_SunHeightAngle Auto

; Community Shaders override target
String CS_OverridePath = "Data/SKSE/Plugins/CommunityShaders/Overrides/SeasonsOfTheSun_SkySync.json"

; ---------------------------------------------------------------------------
; EVENT HANDLERS
; ---------------------------------------------------------------------------

Event OnInit()
    RegisterForPeriodicUpdate()
    EvaluateAngle()
	;Debug.Trace("SeasonsOfTheSun Quest Initialized.")
EndEvent

Event OnPlayerLoadGame()
    RegisterForPeriodicUpdate()
    EvaluateAngle()
EndEvent

Event OnUpdateGameTime()
    EvaluateAngle()
EndEvent

; ---------------------------------------------------------------------------
; REGISTRATION HELPER
; ---------------------------------------------------------------------------

Function RegisterForPeriodicUpdate()
    UnregisterForUpdateGameTime()
    ; Re-evaluates sun angle every 6 game-hours for active testing
    RegisterForUpdateGameTime(6.0)
EndFunction

; ---------------------------------------------------------------------------
; CORE CALCULATION & FILE I/O
; ---------------------------------------------------------------------------

Function EvaluateAngle()
    Float currentDay = Utility.GetCurrentGameTime()
    UpdateSeasonalAngle(currentDay)
EndFunction

Function UpdateSeasonalAngle(Float currentDay)
    Float totalDays = 365.0
    if (UseFasterSeasons)
        totalDays = 121.667
    endif

    ; Float modulo formula in Papyrus:
    Float rawDay = currentDay - ((currentDay / totalDays) as Int * totalDays)
    Int dayOfYear = rawDay as Int

    Int totalDaysInt = totalDays as Int
    Bool invertSouthern = (BaseLatitude > 0.0)

    ; Call C++ Native Math
    Float calculatedAngle = SeasonsOfTheSunNative.GetCalculatedSunAngle(BaseLatitude, dayOfYear, totalDaysInt, invertSouthern)

    if (Seasonal_SunHeightAngle != None)
        Seasonal_SunHeightAngle.SetValue(calculatedAngle)
    endif

    ;Debug.Notification("Seasonal Sun Angle: Updated to " + calculatedAngle + " deg")
EndFunction

; Call this from MCM when settings change
Function UpdateSettings(Float a_latitude, Bool a_useFasterSeasons)
    BaseLatitude = a_latitude
    UseFasterSeasons = a_useFasterSeasons
    EvaluateAngle()
EndFunction