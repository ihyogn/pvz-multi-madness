#ifndef __BUSH_H__
#define __BUSH_H__

#include "GameObject.h"

namespace Sexy
{
    class Graphics;
}
using namespace Sexy;

class Bush : public GameObject
{
public:
    int                     mIndex;
    ReanimationID           mReanimID;

public:
    void                    BushInitialize(int theRow);
    void                    Rustle();
    void                    Update();
    void                    Draw(Graphics* g);
};

#endif
