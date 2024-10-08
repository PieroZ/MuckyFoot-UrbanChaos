#pragma once
//#include <string>
class DebugVars
{
public:
    static DebugVars& getInstance();
    DebugVars(const DebugVars&) = delete;
    void operator=(const DebugVars&) = delete;
    
    void SetInflate();
    void SetMIBExplosions();
    void SetRandomGrapple();
    void SetCorpseExplosion();
    void SetRandomCharacters();
    void SetDisableFloorsRender();
    void SetDisableThingsRender();

    bool GetInflate() const;
    bool GetMIBExplosions() const;
    bool GetRandomGrapple() const;
    bool GetCorpseExplosion() const;
    bool GetRandomCharacters() const;
    bool GetDisableFloorsRender() const;
    bool GetDisableThingsRender() const;

private:
    bool mInflate = false;
    bool mMIBExplosions = false;
    bool mRandomGrapple = false;
    bool mCorpseExplosion = false;
    bool mParasyte = false;
    bool mRandomCharacters = false;
    bool mDisableFloorsRender = false;
    bool mDisableThingsRender = false;

    DebugVars();
};

