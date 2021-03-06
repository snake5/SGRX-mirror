

#pragma once
#include "gameui.hpp"
#include "systems.hpp"


extern struct TSMenuTheme g_TSMenuTheme;
extern struct TSSplashScreen g_SplashScreen;
extern struct TSQuitGameQuestionScreen g_QuitGameQuestionScreen;
extern struct TSPauseMenuScreen g_PauseMenu;


enum TSMenuCtrlStyle
{
	MCS_BigTopLink = 100,
	MCS_ObjectiveItem,
	MCS_ObjectiveItemDone,
	MCS_ObjectiveIconLink,
};


struct TSMenuTheme : MenuTheme
{
	TSMenuTheme();
	virtual void DrawControl( const MenuControl& ctrl, const MenuCtrlInfo& info );
	void DrawBigTopLinkButton( const MenuControl& ctrl, const MenuCtrlInfo& info );
	void DrawObjectiveItemButton( const MenuControl& ctrl, const MenuCtrlInfo& info );
	void DrawObjectiveIconLink( const MenuControl& ctrl, const MenuCtrlInfo& info );
};


struct TSSplashScreen : IScreen
{
	TSSplashScreen();
	void OnStart();
	void OnEnd();
	bool OnEvent( const Event& e );
	bool Draw( float delta );
	
	float m_timer;
	TextureHandle m_tx_crage;
	TextureHandle m_tx_back;
};


struct TSQuestionScreen : IScreen
{
	TSQuestionScreen();
	void OnStart();
	void OnEnd();
	bool OnEvent( const Event& e );
	bool Draw( float delta );
	virtual bool Action( int mode );
	
	float animFactor;
	float animTarget;
	String question;
	ScreenMenu menu;
};


struct TSQuitGameQuestionScreen : TSQuestionScreen
{
	TSQuitGameQuestionScreen();
	virtual bool Action( int mode );
};


#define PMS_OBJ_SCROLLUP 10000
#define PMS_OBJ_SCROLLDN 10001

struct TSPauseMenuScreen : IScreen
{
	TSPauseMenuScreen();
	void OnStart();
	void OnEnd();
	void Unpause();
	bool OnEvent( const Event& e );
	bool Draw( float delta );
	
	void ReloadObjMenu();
	int ScrollObjMenu( int amount );
	
	bool notfirst;
	ScreenMenu topmenu;
	ScreenMenu pausemenu;
	ScreenMenu objmenu;
	bool show_objectives;
	int selobj_id;
	int firstobj;
	OSObjective selected_objective;
};


