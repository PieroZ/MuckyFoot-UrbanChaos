// What
#include "DebugVars.h"

DebugVars::DebugVars()
{

}

DebugVars& DebugVars::getInstance()
{
    static DebugVars instance;
    return instance;
}

void DebugVars::SetInflate()
{
    mInflate = !mInflate;
}

void DebugVars::SetRandomGrapple()
{
    mRandomGrapple = !mRandomGrapple;
}

void DebugVars::SetMIBExplosions()
{
    mMIBExplosions = !mMIBExplosions;
}

void DebugVars::SetCorpseExplosion()
{
    mCorpseExplosion = !mCorpseExplosion;
}

void DebugVars::SetRandomCharacters()
{
    mRandomCharacters = !mRandomCharacters;
}

void DebugVars::SetDisableFloorsRender()
{
    mDisableFloorsRender = !mDisableFloorsRender;
}

void DebugVars::SetDisableThingsRender()
{
    mDisableThingsRender = !mDisableThingsRender;
}

bool DebugVars::GetInflate() const
{
    return mInflate;
}

bool DebugVars::GetRandomGrapple() const
{
    return mRandomGrapple;
}

bool DebugVars::GetMIBExplosions() const
{
    return mMIBExplosions;
}

bool DebugVars::GetCorpseExplosion() const
{
    return mCorpseExplosion;
}

bool DebugVars::GetRandomCharacters() const
{
    return mRandomCharacters;
}

bool DebugVars::GetDisableFloorsRender() const
{
    return mDisableFloorsRender;
}

bool DebugVars::GetDisableThingsRender() const
{
    return mDisableThingsRender;
}

