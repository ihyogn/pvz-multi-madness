#ifndef __CURSORWIDGET_H__
#define __CURSORWIDGET_H__

#include "../../SexyAppFramework/Widget.h"
#include "../../SexyAppFramework/WidgetManager.h"

class LawnApp;

namespace Sexy
{
	class Image;

	class CursorWidget : public Widget
	{
	public:
		LawnApp*				mApp;
		Image*					mImage;
		bool					mDraw;

	public:
		CursorWidget();

		virtual void			Draw(Graphics* g);
		void					SetImage(Image* theImage);
		void					Update();
	};
}

#endif //__CURSORWIDGET_H__