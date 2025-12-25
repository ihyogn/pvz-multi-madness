#include "CursorWidget.h"
#include "../../LawnApp.h"

CursorWidget::CursorWidget()
{
	mApp = gLawnApp;
	mMouseVisible = false;
	mImage = NULL;
	mDraw = false;
	mZOrder = 1000000000;
}

void CursorWidget::Draw(Graphics* g)
{
	if (mImage != NULL && mDraw)
		g->DrawImage(mImage, 0, 0);
}

void CursorWidget::SetImage(Image* theImage)
{
	mImage = theImage;
	if (mImage != NULL)
		Resize(mX, mY, theImage->mWidth, theImage->mHeight);
}

void CursorWidget::Update()
{
	mX = mApp->mWidgetManager->mLastMouseX;
	mY = mApp->mWidgetManager->mLastMouseY;
	mDraw = mApp->mCursorNum != CURSOR_NONE && mApp->mCustomCursor;
}
