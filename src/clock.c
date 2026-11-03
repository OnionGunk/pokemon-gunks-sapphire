#include "global.h"
#include "clock.h"
#include "event_data.h"
#include "rtc.h"
#include "time_events.h"
#include "field_specials.h"
#include "lottery_corner.h"
#include "dewford_trend.h"
#include "tv.h"
#include "field_weather.h"
#include "berry.h"
#include "main.h"
#include "overworld.h"
#include "wallclock.h"

static void UpdatePerDay(struct Time *localTime);
static void UpdatePerMinute(struct Time *localTime);

void InitTimeBasedEvents(void)
{
    FlagSet(FLAG_SYS_CLOCK_SET);
    RtcCalcLocalTime();
    gSaveBlock2.lastBerryTreeUpdate = gLocalTime;
    VarSet(VAR_DAYS, gLocalTime.days);
}

// Gunk: true when time a is before time b
static bool8 GunkTimeBefore(struct Time *a, struct Time *b)
{
    if (a->days != b->days)
        return a->days < b->days;
    if (a->hours != b->hours)
        return a->hours < b->hours;
    if (a->minutes != b->minutes)
        return a->minutes < b->minutes;
    return a->seconds < b->seconds;
}

void DoTimeBasedEvents(void)
{
    if (FlagGet(FLAG_SYS_CLOCK_SET))
    {
        RtcCalcLocalTime();
        // Gunk: on the play-time clock (the RTC reports a power failure), never run behind the last berry update.
        // A battery that dies after the clock was set on a working RTC would otherwise put the clock years back,
        // and berries and daily events would stop; carry on from that time instead.
        if ((RtcGetErrorStatus() & RTC_ERR_FLAG_MASK)
         && GunkTimeBefore(&gLocalTime, &gSaveBlock2.lastBerryTreeUpdate))
        {
            struct Time *last = &gSaveBlock2.lastBerryTreeUpdate;

            RtcCalcLocalTimeOffset(last->days, last->hours, last->minutes, last->seconds);
        }
        UpdatePerDay(&gLocalTime);
        UpdatePerMinute(&gLocalTime);
    }
}

static void UpdatePerDay(struct Time *localTime)
{
    u16 *varPtr = GetVarPointer(VAR_DAYS);
    int days = *varPtr;
    u16 daysSince;

    if (days != localTime->days && days <= localTime->days)
    {
        daysSince = localTime->days - days;
        ClearDailyFlags();
        UpdateDewfordTrendPerDay(daysSince);
        UpdateTVShowsPerDay(daysSince);
        UpdateWeatherPerDay(daysSince);
        UpdatePartyPokerusTime(daysSince);
        UpdateMirageRnd(daysSince);
        UpdateBirchState(daysSince);
        SetShoalItemFlag(daysSince);
        SetRandomLotteryNumber(daysSince);
        *varPtr = localTime->days;
    }
}

static void UpdatePerMinute(struct Time *localTime)
{
    struct Time difference;
    int minutes;

    CalcTimeDifference(&difference, &gSaveBlock2.lastBerryTreeUpdate, localTime);
    minutes = 24 * 60 * difference.days + 60 * difference.hours + difference.minutes;
    if (minutes != 0)
    {
        if (minutes >= 0)
        {
            BerryTreeTimeUpdate(minutes);
            gSaveBlock2.lastBerryTreeUpdate = *localTime;
        }
    }
}

static void ReturnFromStartWallClock(void)
{
    InitTimeBasedEvents();
    SetMainCallback2(CB2_ReturnToFieldContinueScriptPlayMapMusic);
}

void StartWallClock(void)
{
    SetMainCallback2(CB2_StartWallClock);
    gMain.savedCallback = ReturnFromStartWallClock;
}
