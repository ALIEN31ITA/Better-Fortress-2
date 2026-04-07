//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_CONTROLS_H
#define TF_CONTROLS_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/IScheme.h>
#include <vgui/KeyCode.h>
#include <KeyValues.h>
#include <vgui/IVGui.h>
#include <vgui_controls/Panel.h>
#include <vgui_controls/ScrollBar.h>
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/PanelListPanel.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/RichText.h>
#include "utlvector.h"
#include "vgui_controls/PHandle.h"
#include <vgui_controls/Tooltip.h>
#include "econ_controls.h"
#include "sc_hinticon.h"
#include "steam/steam_api.h"
#if defined( TF_CLIENT_DLL )
#include "tf_shareddefs.h"
#include "tf_imagepanel.h"
#endif
#include "vgui_controls/PropertyDialog.h"
#include <vgui_controls/Frame.h>
#include <../common/GameUI/scriptobject.h>
#include <vgui/KeyCode.h>
#include <vgui_controls/Tooltip.h>
#include <vgui_controls/CheckButton.h>
#include <vgui/IImage.h>

wchar_t* LocalizeNumberWithToken( const char* pszLocToken, int nValue );
wchar_t* LocalizeNumberWithToken( const char* pszLocToken, int nValue1, int nValue2 );
void GetPlayerNameForSteamID( wchar_t *wCharPlayerName, int nBufSizeBytes, const CSteamID &steamID );
bool BGeneralPaintSetup( const Color& color );
void DrawFilledColoredCircle( float flXPos, float flYPos, float flRadius, const Color& color );
void DrawFilledColoredCircleSegment( float flXPos, float flYPos, float flRadiusOuter, float flRadiusInner, const Color& color, float flStartAngleOuter, float flEndAngleOuter, bool bCW = true );
void DrawFilledColoredCircleSegment( float flXPos, float flYPos, float flRadiusOuter, float flRadiusInner, const Color& color, float flStartAngleOuter, float flEndAngleOuter, float flStartAngleInner, float flEndAngleInner, bool bCW = true );
void DrawColoredCircle( float flXPos, float flYPos, float flRadius, const Color& color );
void BrigthenColor( Color& color, int nBrigthenAmount );
void CreateSwoop( int nX, int nY, int nWide, int nTall, float flDelay, bool bDown );

enum tooltippos_t
{
	TTP_ABOVE = 0,
	TTP_RIGHT_CENTERED,
	TTP_RIGHT,
	TTP_BELOW,
	TTP_LEFT,
	TTP_LEFT_CENTERED,

	MAX_POSITIONS
};

void PositionTooltip( const tooltippos_t ePreferredTooltipPosition, 
					  vgui::Panel* pMouseOverPanel,
					  vgui::Panel *pToolTipPanel );

//Custom Fortress Res
#define RES_SERVERMENU	"resource/ui/custom/createserver/Menu.res"
#define RES_CREDITSMENU "resource/ui/custom/AuthorCredits.res"

//-----------------------------------------------------------------------------
// Purpose: Xbox-specific panel that displays button icons text labels
//-----------------------------------------------------------------------------
class CTFFooter : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CTFFooter, vgui::EditablePanel );

public:
	CTFFooter( Panel *parent, const char *panelName );
	virtual ~CTFFooter();

	virtual void	ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void	ApplySettings( KeyValues *pResourceData );
	virtual void	Paint( void );
	virtual void	PaintBackground( void );

	void			ShowButtonLabel( const char *name, bool show = true );
	void			AddNewButtonLabel( const char *name, const char *text, const char *icon );
	void			ClearButtons();

private:
	struct FooterButton_t
	{
		bool	bVisible;
		char	name[MAX_PATH];
		wchar_t	text[MAX_PATH];
		wchar_t	icon[3];			// icon can be one or two characters
	};

	CUtlVector< FooterButton_t* > m_Buttons;

	bool			m_bPaintBackground;		// fill the background?
	int				m_nButtonGap;			// space between buttons
	int				m_FooterTall;			// height of the footer
	int				m_ButtonOffsetFromTop;	// how far below the top the buttons should be drawn
	int				m_ButtonSeparator;		// space between the button icon and text
	int				m_TextAdjust;			// extra adjustment for the text (vertically)...text is centered on the button icon and then this value is applied
	bool			m_bCenterHorizontal;	// center buttons horizontally?
	int				m_ButtonPinRight;		// if not centered, this is the distance from the right margin that we use to start drawing buttons (right to left)

	char			m_szTextFont[64];		// font for the button text
	char			m_szButtonFont[64];		// font for the button icon
	char			m_szFGColor[64];		// foreground color (text)
	char			m_szBGColor[64];		// background color (fill color)

	vgui::HFont		m_hButtonFont;
	vgui::HFont		m_hTextFont;
};

//-----------------------------------------------------------------------------
// Purpose: Tooltip for the main menu. Isn't a panel, it just wraps the 
// show/hide/position handling for the embedded panel.
//-----------------------------------------------------------------------------
class CMainMenuToolTip : public vgui::BaseTooltip 
{
	DECLARE_CLASS_SIMPLE( CMainMenuToolTip, vgui::BaseTooltip );
public:
	CMainMenuToolTip(vgui::Panel *parent, const char *text = NULL) : vgui::BaseTooltip( parent, text )
	{
		m_pEmbeddedPanel = NULL;
	}
	virtual ~CMainMenuToolTip() {}

	virtual void SetText(const char *text);
	const char *GetText() { return NULL; }

	virtual void HideTooltip();
	virtual void PerformLayout();

	void SetEmbeddedPanel( vgui::EditablePanel *pPanel )
	{
		m_pEmbeddedPanel = pPanel;
	}

protected:
	vgui::EditablePanel	*m_pEmbeddedPanel;
};

//-----------------------------------------------------------------------------
// Purpose: Simple TF-styled text tooltip
//-----------------------------------------------------------------------------
class CTFTextToolTip : public CMainMenuToolTip 
{
	DECLARE_CLASS_SIMPLE( CTFTextToolTip, CMainMenuToolTip );
public:
	CTFTextToolTip(vgui::Panel *parent, const char *text = NULL) : CMainMenuToolTip( parent, text )
	{
	}
	virtual void PerformLayout();
	virtual void PositionWindow( vgui::Panel *pTipPanel );
	virtual void ShowTooltip( vgui::Panel* pCurrentPanel ) OVERRIDE;
	virtual void SetText(const char *text)
	{
		_isDirty = true;
		BaseClass::SetText( text );
	}
	void SetMaxWide( int nMaxWide ) { m_nMaxWide = YRES( nMaxWide ); }

private:

	int m_nMaxWide = 0;
};


//-----------------------------------------------------------------------------
// Purpose: Displays a TF specific list of options
//			This is essentially a TF-styled version of the GameUI Advanced Multiplayer Options Dialog
//-----------------------------------------------------------------------------
class CTFAdvancedOptionsDialog : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CTFAdvancedOptionsDialog, vgui::EditablePanel ); 

public:
	CTFAdvancedOptionsDialog(vgui::Panel *parent);
	~CTFAdvancedOptionsDialog();

	virtual void	ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void	ApplySettings( KeyValues *pResourceData );

	void	Deploy( void );

private:

	void CreateControls();
	void DestroyControls();
	void GatherCurrentValues();
	void SaveValues();
	void FilterOptions();

	virtual void OnCommand( const char *command );
	virtual void OnClose();
	virtual void OnKeyCodeTyped(vgui::KeyCode code);
	virtual void OnKeyCodePressed(vgui::KeyCode code);
	virtual void OnTextChanged( vgui::Panel *panel );
	virtual void OnThink();

private:
	CInfoDescription	*m_pDescription;
	mpcontrol_t			*m_pList;
	vgui::PanelListPanel *m_pListPanel;
	CTFTextToolTip		*m_pToolTip;
	vgui::EditablePanel	*m_pToolTipEmbeddedPanel;
	vgui::TextEntry		*m_pSearchEntry;
	char				m_szLastSearchFilter[256];

	CPanelAnimationVarAliasType( int, m_iControlW, "control_w", "0", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iControlH, "control_h", "0", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iSliderW, "slider_w", "0", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iSliderH, "slider_h", "0", "proportional_int" );
};

//-----------------------------------------------------------------------------
// Purpose: Displays scrollable mod credits window
//-----------------------------------------------------------------------------
class CTFModCreditsDialog : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE(CTFModCreditsDialog, vgui::EditablePanel);

public:
	CTFModCreditsDialog(vgui::Panel* parent);
	~CTFModCreditsDialog();

	virtual void	ApplySchemeSettings(vgui::IScheme* pScheme);
	virtual void	ApplySettings(KeyValues* pResourceData);

	void	Deploy(void);

private:

	void CreateControls();
	void DestroyControls();
	void GatherCurrentValues();

	virtual void OnCommand(const char* command);
	virtual void OnClose();
	virtual void OnKeyCodeTyped(vgui::KeyCode code);
	virtual void OnKeyCodePressed(vgui::KeyCode code);

private:
	CInfoDescription* m_pDescription;
	mpcontrol_t* m_pList;
	vgui::PanelListPanel* m_pListPanel;
	CTFTextToolTip* m_pToolTip;
	vgui::EditablePanel* m_pToolTipEmbeddedPanel;

	CPanelAnimationVarAliasType(int, m_iControlW, "control_w", "0", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iControlH, "control_h", "0", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iSliderW, "slider_w", "0", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iSliderH, "slider_h", "0", "proportional_int");
};

namespace vgui
{
//-----------------------------------------------------------------------------
// Purpose: Simple IImage wrapper for workshop map preview textures
//-----------------------------------------------------------------------------
class CMapPreviewImage : public IImage
{
public:
	CMapPreviewImage();
	virtual ~CMapPreviewImage();
	
	void SetTextureRGBA(const byte* rgba, int width, int height);
	void Clear();
	bool IsValid() const { return m_bValid; }
	
	// IImage interface
	virtual void Paint() override;
	virtual void SetPos(int x, int y) override { m_nX = x; m_nY = y; }
	virtual void GetContentSize(int &wide, int &tall) override { wide = m_nWide; tall = m_nTall; }
	virtual void GetSize(int &wide, int &tall) override { wide = m_nWide; tall = m_nTall; }
	virtual void SetSize(int wide, int tall) override { m_nWide = wide; m_nTall = tall; }
	virtual void SetColor(Color col) override { m_Color = col; }
	virtual bool Evict() override { return false; }
	virtual int GetNumFrames() override { return 1; }
	virtual void SetFrame(int nFrame) override {}
	virtual HTexture GetID() override { return m_nTextureID; }
	virtual void SetRotation(int iRotation) override {}

private:
	int m_nTextureID;
	int m_nX, m_nY;
	int m_nWide, m_nTall;
	int m_nImageWidth, m_nImageHeight;
	Color m_Color;
	bool m_bValid;
};
} // namespace vgui

struct CreateServerMapItem {
	char mapName[MAX_MAP_NAME];
	bool bIsWorkshopMap;
	PublishedFileId_t iMapFileId;
};

class CTFCreateServerDialog : public vgui::PropertyDialog
{
	DECLARE_CLASS_SIMPLE(CTFCreateServerDialog, vgui::PropertyDialog);

public:
	CTFCreateServerDialog(vgui::Panel* parent);
	~CTFCreateServerDialog();

	virtual void	ApplySchemeSettings(vgui::IScheme* pScheme);
	virtual void	ApplySettings(KeyValues* pResourceData);

	void	Deploy(void);

private:

	void CreateControls();
	void DestroyControls();
	void GatherCurrentValues();
	void SaveValues();
	void LoadMapList();
	void RefreshMapList();
	void RequestWorkshopPreview(PublishedFileId_t fileID);
	void FilterOptions();

	// HTTP callback for workshop preview image
	CCallResult<CTFCreateServerDialog, HTTPRequestCompleted_t> m_callbackHTTPPreview;
	void Steam_OnPreviewImageReceived(HTTPRequestCompleted_t* pResult, bool bError);

	virtual void OnCommand( const char *command );
	virtual void OnClose();
	virtual void OnKeyCodeTyped(vgui::KeyCode code);
	virtual void OnKeyCodePressed(vgui::KeyCode code);
	virtual void OnThink();

	MESSAGE_FUNC_PTR( OnTextChanged, "TextChanged", panel );
	MESSAGE_FUNC_INT( OnCheckButtonChecked, "CheckButtonChecked", state );

private:
	CInfoDescription	*m_pDescription;
	mpcontrol_t* m_pList;
	CTFTextToolTip* m_pToolTip;
	vgui::EditablePanel* m_pToolTipEmbeddedPanel;

	CUtlVector< vgui::PanelListPanel* > m_pPages;

	// Map filtering
	vgui::TextEntry* m_pMapSearchEntry;
	vgui::CheckButton* m_pWorkshopFilterCheck;
	//CUtlVector< CUtlString > m_vecAllMaps;
	//CUtlVector< bool > m_vecIsWorkshopMap;
	//CUtlVector< PublishedFileId_t > m_vecMapFileIDs;
	CUtlVector<CreateServerMapItem> m_vecAllMaps;
	char m_szLastSearchFilter[256];
	bool m_bLastWorkshopOnly;

	// General options filtering
	vgui::TextEntry* m_pOptionsSearchEntry;
	char m_szLastOptionsSearchFilter[256];

	// Workshop preview image
	HTTPRequestHandle m_hPendingPreviewRequest;
	PublishedFileId_t m_nCurrentPreviewFileID;
	vgui::CMapPreviewImage* m_pWorkshopPreviewImage;
	PublishedFileId_t m_nLastDisplayedMapFileID;

	CPanelAnimationVarAliasType(int, m_iControlW, "control_w", "0", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iControlH, "control_h", "20", "proportional_int"); // Assume they want height 20
	CPanelAnimationVarAliasType(int, m_iSliderW, "slider_w", "0", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iSliderH, "slider_h", "0", "proportional_int");
};

//-----------------------------------------------------------------------------
// Purpose: Scrollable panel where you can define children within the .res file
//-----------------------------------------------------------------------------
class CExScrollingEditablePanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CExScrollingEditablePanel, vgui::EditablePanel );
public:
	CExScrollingEditablePanel( Panel *pParent, const char *pszName );
	virtual ~CExScrollingEditablePanel();

	virtual void ApplySettings( KeyValues *inResourceData ) OVERRIDE;
	virtual void PerformLayout() OVERRIDE;
	virtual void OnSizeChanged( int newWide, int newTall ) OVERRIDE;

	MESSAGE_FUNC( OnScrollBarSliderMoved, "ScrollBarSliderMoved" );
	virtual void OnMouseWheeled( int delta ) OVERRIDE;	// respond to mouse wheel events
	void ResetScrollAmount() { m_nLastScrollValue = 0; m_pScrollBar->SetValue(0); }
	int GetScrollAmount() const { return m_nLastScrollValue; }
protected:

	void ShiftChildren( int nDistance );

	vgui::ScrollBar *m_pScrollBar;
	int m_nLastScrollValue;
	bool m_bUseMouseWheelToScroll;
	CPanelAnimationVarAliasType( int, m_iScrollStep, "scroll_step", "10", "proportional_xpos" );
	CPanelAnimationVarAliasType( int, m_iBottomBuffer, "bottom_buffer", "0", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_bRestrictWidth, "restrict_width", "1", "proportional_int" );
};

//-----------------------------------------------------------------------------
// An extension of CExScrollingEditablePanel where panels can be added to form
// a list.
//-----------------------------------------------------------------------------
class CScrollableList : public CExScrollingEditablePanel
{
	DECLARE_CLASS_SIMPLE( CScrollableList, CExScrollingEditablePanel );
public:
	CScrollableList( Panel* pParent, const char* pszName )
		: CExScrollingEditablePanel( pParent, pszName )
	{}

	virtual ~CScrollableList();

	virtual void PerformLayout() OVERRIDE;

	void AddPanel( Panel* pPanel, int nGap );
	void ClearAutoLayoutPanels();

private:

	struct LayoutInfo_t
	{
		Panel* m_pPanel;
		int m_nGap;
	};
	
	CUtlVector< LayoutInfo_t > m_vecAutoLayoutPanels;
};

//-----------------------------------------------------------------------------
// A checkbox where keyvalue data can be stored and retrieved (typically in OnCheckButtonChecked)
// so that you don't need a pointer to the checkbox in order to determine WHICH
// checkbox got checked.
//-----------------------------------------------------------------------------
class CExCheckButton : public vgui::CheckButton
{
	DECLARE_CLASS_SIMPLE( CExCheckButton, vgui::CheckButton );
public:
	CExCheckButton( Panel* pParent, const char* pszName )
		: BaseClass( pParent, pszName, NULL )
		, m_pKVData( NULL )
	{}

	virtual ~CExCheckButton()
	{
		if ( m_pKVData )
			m_pKVData->deleteThis();
	}

	void SetData( KeyValues* pKVData )
	{
		if ( m_pKVData )
		{
			m_pKVData->deleteThis();
			m_pKVData = NULL;
		}

		m_pKVData = pKVData;
	}

	KeyValues* GetData() const
	{
		return m_pKVData;
	}

private:
	KeyValues *m_pKVData;
};

class CExpandablePanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CExpandablePanel, vgui::EditablePanel );
public:

	enum EExpandDir_t
	{
		EXPAND_DOWN,
		EXPAND_UP,
		EXPAND_LEFT,
		EXPAND_RIGHT
	};

	CExpandablePanel( Panel* pParent, const char* pszName );

	virtual void ApplySettings( KeyValues *inResourceData ) OVERRIDE;
	virtual void OnCommand( const char *command ) OVERRIDE;
	virtual void OnThink() OVERRIDE;

	virtual void OnToggleCollapse( bool bIsExpanded ) {}

	void SetCollapsed( bool bCollapsed, bool bInstant = false );
	void ToggleCollapse();
	bool BIsExpanded() const { return m_bExpanded; }
	void SetExpandedHeight( int nNewHeight ) { m_nExpandedHeight = nNewHeight; }
	void SetCollapsedHeight( int nNewHeight ) { m_nCollapsedHeight = nNewHeight; }
	float GetPercentAnimated() const;
	float GetPercentExpanded() const;

	int GetExpandedHeight() const { return m_nExpandedHeight; }
	int GetCollapsedHeight() const { return m_nCollapsedHeight; }

protected:

	int GetDimension();
	void SetDimension( int nNewValue );

	CPanelAnimationVarAliasType( float, m_flResizeTime, "resize_time", "0.4", "float" );
	CPanelAnimationVarAliasType( int, m_nCollapsedHeight, "collapsed_height", "17", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_nExpandedHeight, "expanded_height", "50", "proportional_int" );

private:

	bool bInvalidateParentOnResize = true;
	EExpandDir_t m_eExpandDir = EXPAND_DOWN;
	bool m_bExpanded;
	float m_flAnimEndTime;
};

//-----------------------------------------------------------------------------
// Purpose: A panel that can dragged and zoomed
//-----------------------------------------------------------------------------
class CDraggableScrollingPanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CDraggableScrollingPanel, vgui::EditablePanel );
public:

	enum EPinPosition
	{
		PIN_TOP_LEFT = 0,
		PIN_TOP_RIGHT,
		PIN_BOTTOM_LEFT,
		PIN_BOTTOM_RIGHT,
		PIN_CENTER
	};

	// Child panel positions as a function of our width and height so that
	// when we scale the background, we can reposition these panels
	struct ChildPositionInfo_t
	{
		Panel* m_pChild;
		CUtlString m_strName;
		float m_flX;
		float m_flY;
		float m_flWide;
		float m_flTall;
		bool m_bScaleWithZoom;
		bool m_bMoveWithDrag;
		bool m_bWaitingForSettings;
		EPinPosition m_ePinPosition;
	};

	CDraggableScrollingPanel( Panel *pParent, const char *pszPanelname );

	virtual void ApplySettings( KeyValues *inResourceData ) OVERRIDE;
	virtual void OnChildRemoved( Panel* pChild ) OVERRIDE;
	virtual void OnTick() OVERRIDE;

	virtual void OnMousePressed( vgui::MouseCode code ) OVERRIDE;
	virtual void OnMouseReleased( vgui::MouseCode code ) OVERRIDE;
	virtual void OnMouseWheeled( int delta ) OVERRIDE;

	MESSAGE_FUNC_INT_INT( InternalCursorMoved, "CursorMoved", xpos, ypos );
	MESSAGE_FUNC_PARAMS( OnSliderMoved, "SliderMoved", pParams );

	void AddOrUpdateChild( Panel* pChild, bool bScaleWithZoom, bool bMoveWithDrag, EPinPosition ePinPosition );
	void SetZoomAmount( float flZoomAmount, int nXZoomFocus, int nYZoomFocus );
	float GetZoomAmount() const { return m_flZoom; }

	const ChildPositionInfo_t* GetChildPositionInfo( const Panel* pChildPanel ) const;

private:

	bool BCheckForPendingChildren();
	virtual void OnChildSettingsApplied( KeyValues *pInResourceData, Panel *pChild ) OVERRIDE;
	void UpdateChildren();
	void CaptureChildSettings( Panel* pChild );

	CUtlVector< ChildPositionInfo_t > m_vecChildOriginalData;
	CUtlVector< ChildPositionInfo_t > m_vecPendingChildren;

	float m_flMinZoom;
	float m_flMaxZoom;
	float m_flZoom;
	float m_flMouseWheelZoomRate;

	int m_iOriginalWide;
	int m_iOriginalTall;
	int m_iDragStartX;
	int m_iDragStartY;
	int m_iDragTotalDistance;
	bool m_bDragging;
};

class CTFLogoPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CTFLogoPanel, vgui::Panel );
public:
	CTFLogoPanel( Panel *pParent, const char *pszPanelname );

	virtual void Paint() OVERRIDE;

protected:
	CPanelAnimationVarAliasType( float, m_flRadius, "radius", "5", "float" );
	CPanelAnimationVarAliasType( float, m_flVelocity, "velocity", "0", "float" );

private:
	void PaintTFLogo( float flAngle, const Color& color ) const;

	float m_flOffsetAngle = 0.f;
};

void CreateScrollingIndicator( int nXPos,
							   int nYPos,
							   const wchar* pwszText,
							   const char* pszSoundName,
							   float flDelay,
							   int nXTravel,
							   int nYTravel, 
							   bool bPositive );

// Helper to create a string that can blame users for some action.
// Example: They don't have a Widget.
//			"User1 doesn't have a Widget"
//			"User1 and User2 don't have a Widget"
//			"User1, User2, and User3 don't have a Widget"
struct BlameNames_t
{
public:
	BlameNames_t( const CUtlVector< CSteamID >& vecBlameSteamIDs, const char* pszReason, const char* pszSingularVerb, const char* pszPluralVerb )
	{
		wchar_t wszMembers[ 512 ];

		FOR_EACH_VEC( vecBlameSteamIDs, i )
		{
			if ( i == 0 )
			{
				g_pVGuiLocalize->ConstructString_safe( wszMembers, L"%s1", 1, CStrAutoEncode( SteamFriends()->GetFriendPersonaName( vecBlameSteamIDs[ i ] ) ).ToWString() );
			}
			else if ( i == vecBlameSteamIDs.Count() - 1 )
			{
				g_pVGuiLocalize->ConstructString_safe( wszMembers, g_pVGuiLocalize->Find( "#TF_PartyMemberState_LastTwo" ), 2, CStrAutoEncode( wszMembers ).ToWString(), CStrAutoEncode( SteamFriends()->GetFriendPersonaName( vecBlameSteamIDs[ i ] ) ).ToWString() );
			}
			else
			{
				g_pVGuiLocalize->ConstructString_safe( wszMembers, L"%s1, %s2", 2, CStrAutoEncode( wszMembers ).ToWString(), CStrAutoEncode( SteamFriends()->GetFriendPersonaName( vecBlameSteamIDs[ i ] ) ).ToWString() );
			}
		}

		g_pVGuiLocalize->ConstructString_safe( wszMembers, vecBlameSteamIDs.Count() == 1 ? g_pVGuiLocalize->Find( pszSingularVerb ) : g_pVGuiLocalize->Find( pszPluralVerb ) , 1, CStrAutoEncode( wszMembers ).ToWString() );
		g_pVGuiLocalize->ConstructString_safe( m_wszBuff, g_pVGuiLocalize->Find( pszReason ), 1, wszMembers );
	}

	const wchar_t* Get() const { return m_wszBuff; }
private:
	wchar_t m_wszBuff[ 1024 ];
};
#endif // TF_CONTROLS_H
