#include "Bush.h"
#include "Board.h"
#include "../LawnApp.h"
#include "../Resources.h"
#include "../GameConstants.h"
#include "../Sexy.TodLib/TodFoley.h"
#include "../Sexy.TodLib/TodDebug.h"
#include "../Sexy.TodLib/Reanimator.h"

const ReanimationType cBushReanims[] = { ReanimationType::REANIM_BUSH3, ReanimationType::REANIM_BUSH5, ReanimationType::REANIM_BUSH4, ReanimationType::REANIM_BUSH3_NIGHT, ReanimationType::REANIM_BUSH5_NIGHT, ReanimationType::REANIM_BUSH4_NIGHT };

const int cBushPos[][2] = {
    { 950, 40 },
    { 962, 168 },
    { 968, 258 },
    { 972, 378 },
    { 964, 459 },
    { 980, 510 }
};
const int cBushPos6Rows[][2] = {
    { 952, 42 },
    { 964, 170 },
    { 968, 258 },
    { 974, 380 },
    { 966, 461 },
    { 979, 509 }
};

void Bush::BushInitialize(int theRow)
{
    mIndex = theRow;
    mX = mBoard->StageHas6Rows() ? cBushPos6Rows[mIndex][0] : cBushPos[mIndex][0];
    mY = mBoard->StageHas6Rows() ? cBushPos6Rows[mIndex][1] : cBushPos[mIndex][1];
    int aBushIndex = (mIndex + 3) % 3;
    if (mBoard->StageIsNight())
        aBushIndex += 3;
    mRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_ZOMBIE, mIndex + 1, 0);
    Reanimation* aBodyReanim = mApp->AddReanimation(mX, mY, mRenderOrder, cBushReanims[aBushIndex]);
    mReanimID = mApp->ReanimationGetID(aBodyReanim);
    aBodyReanim->mLoopType = REANIM_PLAY_ONCE_AND_HOLD;
    aBodyReanim->mFrameStart = 0;
    aBodyReanim->mFrameCount = 1;
}

void Bush::Rustle()
{
    Reanimation* aReanim = mApp->ReanimationTryToGet(mReanimID);
    if (aReanim)
        aReanim->PlayReanim("anim_rustle", REANIM_PLAY_ONCE_AND_HOLD, 10, RandRangeFloat(8.0f, 10.0f));
}

void Bush::Update()
{
    Reanimation* aReanim = mApp->ReanimationTryToGet(mReanimID);
    if (aReanim)
        aReanim->Update();
}

void Bush::Draw(Graphics* g) {
    Reanimation* aReanim = mApp->ReanimationTryToGet(mReanimID);
    if (aReanim)
        aReanim->Draw(g);
}