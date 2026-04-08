// RadialMenu.h
// Copyright (c) 2006 Turtle Rock Studios, Inc.

#ifndef RADIALMENU_H
#define RADIALMENU_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/EditablePanel.h>
#include "KeyValues.h"

#define PANEL_RADIAL_MENU			"RadialMenu"

class CRadialButton;

//--------------------------------------------------------------------------------------------------------
/**
 * Viewport panel that gives us a simple rosetta menu
 */
class CRadialMenu : public CHudElement, public vgui::EditablePanel
{
private:
	DECLARE_CLASS_SIMPLE( CRadialMenu, vgui::EditablePanel );

public:
	CRadialMenu( const char *pElementName );
	virtual ~CRadialMenu();

	virtual void SetData( KeyValues *data );
	virtual KeyValues *GetData( void ) { return NULL; }
	bool		 IsFading( void ) { return IsVisible() && m_fading; }
	void		 StartFade( void );
	void		 ChooseArmedButton( void );

	virtual const char *GetName( void ) { return PANEL_RADIAL_MENU; }
	virtual void Reset( void ) {}
	virtual void Update( void );
	virtual bool NeedsUpdate( void ) { return false; }
	virtual bool HasInputElements( void ) { return true; }
	virtual void ShowPanel( bool bShow );

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PerformLayout( void );
	void InitializeInputScheme();
	virtual void Paint( void );
	virtual void OnCommand( const char *command );
	virtual vgui::Panel *CreateControlByName( const char *controlName );
	virtual void OnThink( void );

	void OnCursorEnteredButton( int x, int y, CRadialButton *button );

	enum ButtonDir
	{
		CENTER = 0,
		NORTH,
		NORTH_EAST,
		EAST,
		SOUTH_EAST,
		SOUTH,
		SOUTH_WEST,
		WEST,
		NORTH_WEST,
		NUM_BUTTON_DIRS
	};

protected:

	void SendCommand( const char *commandStr ) const;
	const char *ButtonNameFromDir( ButtonDir dir );
	ButtonDir DirFromButtonName( const char * name );
	CRadialButton *m_buttons[NUM_BUTTON_DIRS];	// same order as vgui::Label::Alignment
	ButtonDir m_armedButtonDir;
	void SetArmedButtonDir( ButtonDir dir );
	void UpdateButtonBounds( void );

	void HandleControlSettings();

	int m_cursorX;
	int m_cursorY;

	bool m_fading;
	float m_fadeStart;

	Color m_lineColor;

	KeyValues *m_resource;
	KeyValues *m_menuData;

	int m_minButtonX;
	int m_minButtonY;
	int m_maxButtonX;
	int m_maxButtonY;

public:

	int m_cursorOriginalX;
	int m_cursorOriginalY;
};

void OpenRadialMenu( const char *menuName );
void CloseRadialMenu( const char *menuName, bool sendCommand = false );
bool IsRadialMenuOpen( const char *menuName, bool includeFadingOut );

//--------------------------------------------------------------------------------------------------------
/**
 * Button class that clips its visible bounds to an concave polygon (defined counter-clockwise).
 * An optional second hotspot can be defined for capturing mouse input outside of the visible hotspot.
 */
class CPolygonButton : public vgui::Button
{
	DECLARE_CLASS_SIMPLE( CPolygonButton, vgui::Button );

public:

	CPolygonButton( vgui::Panel *parent, const char *panelName );

	virtual void ApplySettings( KeyValues *data );

	/**
	 * Clip out cursor positions inside our overall rectangle that are outside our hotspot.
	 */
	virtual vgui::VPANEL IsWithinTraverse( int x, int y, bool traversePopups );

	/**
	 * Perform the standard layout, and scale our hotspot points - they are specified as a 0..1 percentage
	 * of the button's full size.
	 */
	virtual void PerformLayout( void );

	/**
	 * Center the text in the extent that encompasses our hotspot.
	 * TODO: allow the res file and/or the individual menu to specify a different center for text.
	 */
	virtual void ComputeAlignment( int &tx0, int &ty0, int &tx1, int &ty1 );
	virtual void PaintBackground( void );
	virtual void PaintBorder( void );
	virtual void ApplySchemeSettings( vgui::IScheme *scheme );

	virtual void UpdateHotspots( KeyValues *data );

protected:
	int m_nWhiteMaterial;

	CUtlVector< Vector2D > m_unscaledHotspotPoints;
	CUtlVector< Vector2D > m_unscaledVisibleHotspotPoints;
	vgui::Vertex_t *m_hotspotPoints;
	int m_numHotspotPoints;
	vgui::Vertex_t *m_visibleHotspotPoints;
	int m_numVisibleHotspotPoints;

	Vector2D m_hotspotMins;
	Vector2D m_hotspotMaxs;
};

#endif // RADIALMENU_H