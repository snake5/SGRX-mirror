

#pragma once
#include "engine.hpp"
#include "script.hpp"



#define GUI_ScrMode_Abs 0
#define GUI_ScrMode_Fit 1
#define GUI_ScrMode_Crop 2

#define GUI_Event_MouseMove  10
#define GUI_Event_MouseEnter 11
#define GUI_Event_MouseLeave 12
#define GUI_Event_BtnDown    13
#define GUI_Event_BtnUp      14
#define GUI_Event_BtnClick   15
#define GUI_Event_MouseWheel 16
#define GUI_Event_PropEdit   21
#define GUI_Event_PropChange 22
#define GUI_Event_SetFocus   31
#define GUI_Event_LoseFocus  32
#define GUI_Event_KeyDown    41
#define GUI_Event_KeyUp      42
#define GUI_Event_TextInput  43
#define GUI_Event_User       256

#define GUI_Key_Unknown   0
#define GUI_Key_Left      1  // shift makes selection stick
#define GUI_Key_Right     2  // ^
#define GUI_Key_Up        3  // ^
#define GUI_Key_Down      4  // ^
#define GUI_Key_DelLeft   5
#define GUI_Key_DelRight  6
#define GUI_Key_Tab       7  // shift makes focus move backwards
#define GUI_Key_Cut       8
#define GUI_Key_Copy      9
#define GUI_Key_Paste     10
#define GUI_Key_Undo      11
#define GUI_Key_Redo      12
#define GUI_Key_SelectAll 13
#define GUI_Key_PageUp    14
#define GUI_Key_PageDown  15
#define GUI_Key_Enter     16
#define GUI_Key_Activate  17
#define GUI_Key_Escape    18

#define GUI_MB_Left 0
#define GUI_MB_Right 1

#define GUI_KeyMod_Filter 0x0FF
#define GUI_KeyMod_Shift  0x100

#define GUI_EVENT_ISBUTTON(x) ((x)==GUI_Event_BtnDown \
	||(x)==GUI_Event_BtnUp||(x)==GUI_Event_BtnClick)
#define GUI_EVENT_ISMOUSE(x) ((x)==GUI_Event_MouseMove \
	||(x)==GUI_Event_MouseEnter||(x)==GUI_Event_MouseLeave \
	||(x)==GUI_Event_MouseWheel||GUI_EVENT_ISBUTTON(x))
#define GUI_EVENT_ISKEY(x) ((x)==GUI_Event_KeyDown||(x)==GUI_Event_KeyUp)


struct GameUIControl;


struct GameUIEvent
{
	SGS_OBJECT_LITE;
	
	SGS_PROPERTY int type;
	SGS_PROPERTY sgsHandle< GameUIControl > target;
	union
	{
		struct {
			int x, y, button;
		} mouse;
		struct {
			int key, engkey, engmod;
			bool repeat;
		} key;
		struct {
			char text[8];
		} text;
	};
	
	FINLINE bool IsMouseEvent() const { return GUI_EVENT_ISMOUSE(type); }
	FINLINE bool IsButtonEvent() const { return GUI_EVENT_ISBUTTON(type); }
	FINLINE bool IsKeyEvent() const { return GUI_EVENT_ISKEY(type); }
	SGS_PROPERTY_FUNC( READ SOURCE mouse.x VALIDATE IsMouseEvent() ) SGS_ALIAS( int x );
	SGS_PROPERTY_FUNC( READ SOURCE mouse.y VALIDATE IsMouseEvent() ) SGS_ALIAS( int y );
	SGS_PROPERTY_FUNC( READ SOURCE mouse.button VALIDATE IsButtonEvent() ) SGS_ALIAS( int button );
	SGS_PROPERTY_FUNC( READ SOURCE key.key VALIDATE IsKeyEvent() ) SGS_ALIAS( int key );
	SGS_PROPERTY_FUNC( READ SOURCE key.engkey VALIDATE IsKeyEvent() ) SGS_ALIAS( int engkey );
	SGS_PROPERTY_FUNC( READ SOURCE key.engmod VALIDATE IsKeyEvent() ) SGS_ALIAS( int engmod );
	SGS_PROPERTY_FUNC( READ SOURCE key.repeat VALIDATE IsKeyEvent() ) SGS_ALIAS( bool repeat );
};
SGS_DEFAULT_LITE_OBJECT_INTERFACE( GameUIEvent );


struct GameUISystem
{
	GameUISystem();
	~GameUISystem();
	void Load( const StringView& sv );
	void EngineEvent( const Event& eev );
	void Draw( float dt );
	
	void _HandleMouseMove( bool optional );
	GameUIControl* _GetItemAtPosition( int x, int y );
	void _OnRemove( GameUIControl* ctrl );
	
	void PrecacheTexture( const StringView& texname );
	
	GameUIControl* m_rootCtrl;
	ScriptContext m_scriptCtx;
	Array< GameUIControl* > m_hoverTrail;
	GameUIControl* m_hoverCtrl;
	GameUIControl* m_kbdFocusCtrl;
	GameUIControl* m_clickCtrl[2];
	int m_mouseX;
	int m_mouseY;
	sgsString m_str_onclick;
	sgsString m_str_onmouseenter;
	sgsString m_str_onmouseleave;
	
	Array< TextureHandle > m_precachedTextures;
};


struct GameUIControl
{
	typedef sgsHandle< GameUIControl > Handle;
	
	SGS_OBJECT;
	
	SGS_PROPERTY int mode;
	SGS_PROPERTY float x;
	SGS_PROPERTY float y;
	SGS_PROPERTY float width;
	SGS_PROPERTY float height;
	SGS_PROPERTY float xalign;
	SGS_PROPERTY float yalign;
	SGS_PROPERTY float xscale;
	SGS_PROPERTY float yscale;
	float _getSWidth(){ return width * xscale; }
	SGS_PROPERTY_FUNC( READ _getSWidth ) float swidth;
	float _getSHeight(){ return height * yscale; }
	SGS_PROPERTY_FUNC( READ _getSHeight ) float sheight;
	SGS_PROPERTY float rx0;
	SGS_PROPERTY float ry0;
	SGS_PROPERTY float rx1;
	SGS_PROPERTY float ry1;
	SGS_PROPERTY float z;
	SGS_PROPERTY sgsVariable metadata;
	SGS_PROPERTY_FUNC( READ ) Handle parent;
	SGS_PROPERTY sgsVariable eventCallback;
	SGS_PROPERTY_FUNC( READ ) sgsVariable shaders; // array
	
	SGS_PROPERTY bool hover;
	bool _getClicked() const { return this == m_system->m_clickCtrl[0] ||
		this == m_system->m_clickCtrl[1]; }
	SGS_PROPERTY_FUNC( READ _getClicked ) SGS_ALIAS( bool clicked );
	bool _getClickedL() const { return this == m_system->m_clickCtrl[0]; }
	SGS_PROPERTY_FUNC( READ _getClickedL ) SGS_ALIAS( bool clickedL );
	bool _getClickedR() const { return this == m_system->m_clickCtrl[1]; }
	SGS_PROPERTY_FUNC( READ _getClickedR ) SGS_ALIAS( bool clickedR );
	
	struct GameUISystem* m_system;
	Array< GameUIControl* > m_subitems;
	
	GameUIControl();
	~GameUIControl();
	static GameUIControl* Create( SGS_CTX );
	int OnEvent( const GameUIEvent& e );
	void BubblingEvent( const GameUIEvent& e );
	void Draw( float dt );
	
	SGS_METHOD void AddCallback( sgsString key, sgsVariable func );
	SGS_METHOD void RemoveCallback( sgsString key, sgsVariable func );
	SGS_METHOD void InvokeCallbacks( sgsString key );
	
	bool Hit( int x, int y );
	SGS_METHOD float IX( float x );
	SGS_METHOD float IY( float y );
	SGS_METHOD float IS( float s );
	SGS_METHOD float InvIX( float x );
	SGS_METHOD float InvIY( float y );
	SGS_METHOD float InvIS( float s );
	Handle GetHandle(){ return Handle( this ); }
	
	// ---
	SGS_IFUNC( GETINDEX ) int _getindex( SGS_ARGS_GETINDEXFUNC );
	SGS_IFUNC( SETINDEX ) int _setindex( SGS_ARGS_SETINDEXFUNC );
	
	SGS_METHOD Handle CreateScreen( int mode, float width, float height,
		float xalign /* = 0 */, float yalign /* = 0 */, float x /* = 0 */, float y /* = 0 */ );
	SGS_METHOD Handle CreateControl( float x, float y, float width, float height );
	
	SGS_METHOD void DReset();
	SGS_METHOD void DCol( float a, float b, float c, float d );
	SGS_METHOD void DTex( StringView name );
	SGS_METHOD void DQuad( float x0, float y0, float x1, float y1 );
	SGS_METHOD void DQuadExt( float x0, float y0, float x1, float y1,
		float tox, float toy, float tsx /* = 1 */, float tsy /* = 1 */ );
	SGS_METHOD void DButton( float x0, float y0, float x1, float y1, Vec4 bdr, Vec4 texbdr );
	SGS_METHOD void DFont( StringView name, float size );
	SGS_METHOD void DText( StringView text, float x, float y, int ha, int va );
};


