//====== Copyright Custom Fortress 2, All rights reserved. =================
//
// Workshop Browser UI Panel
//
//=============================================================================

#ifndef CF_WORKSHOP_PANEL_H
#define CF_WORKSHOP_PANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/Frame.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/ListPanel.h>
#include <vgui_controls/SectionedListPanel.h>
#include <vgui_controls/TextEntry.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/Menu.h>
#include <vgui_controls/ProgressBar.h>
#include <vgui_controls/ScrollableEditablePanel.h>
#include <vgui_controls/Image.h>
#include "cf/cf_workshop_manager.h"
#include "steam/isteamhttp.h"

namespace vgui
{

//-----------------------------------------------------------------------------
// Simple IImage wrapper for workshop preview textures
//-----------------------------------------------------------------------------
class CWorkshopPreviewImage : public IImage
{
public:
	CWorkshopPreviewImage();
	virtual ~CWorkshopPreviewImage();
	
	// Set the texture from RGBA data
	void SetTextureRGBA(const byte* rgba, int width, int height);
	// Load texture from file path
	bool SetTextureFromFile(const char* pszFilePath);
	void Clear();
	bool IsValid() const { return m_nTextureID != -1 && m_bValid; }
	
	// IImage implementation
	virtual void Paint() OVERRIDE;
	virtual void SetPos(int x, int y) OVERRIDE { m_nX = x; m_nY = y; }
	virtual void GetContentSize(int &wide, int &tall) OVERRIDE { wide = m_nWide; tall = m_nTall; }
	virtual void GetSize(int &wide, int &tall) OVERRIDE { wide = m_nWide; tall = m_nTall; }
	virtual void SetSize(int wide, int tall) OVERRIDE { m_nWide = wide; m_nTall = tall; }
	virtual void SetColor(Color col) OVERRIDE { m_Color = col; }
	virtual bool Evict() OVERRIDE { return false; }
	virtual int GetNumFrames() OVERRIDE { return 1; }
	virtual void SetFrame(int nFrame) OVERRIDE {}
	virtual HTexture GetID() OVERRIDE { return m_nTextureID; }
	virtual void SetRotation(int iRotation) OVERRIDE {}

private:
	int m_nTextureID;
	int m_nX, m_nY;
	int m_nWide, m_nTall;
	int m_nImageWidth, m_nImageHeight;
	Color m_Color;
	bool m_bValid;
};

//-----------------------------------------------------------------------------
// Workshop item details panel
//-----------------------------------------------------------------------------
class CCFWorkshopItemDetails : public EditablePanel
{
	DECLARE_CLASS_SIMPLE(CCFWorkshopItemDetails, EditablePanel);

public:
	CCFWorkshopItemDetails(Panel* parent, const char* panelName);
	virtual ~CCFWorkshopItemDetails();

	void SetItem(CCFWorkshopItem* pItem);
	void Clear();
	void SetParentContainer(EditablePanel* pContainer);
	void RefreshButtonStates();
	void RequestPreviewImage(const char* pszURL);

	virtual void ApplySchemeSettings(IScheme* pScheme) OVERRIDE;
	virtual void OnCommand(const char* command) OVERRIDE;
	virtual void OnTick() OVERRIDE;

private:
	// HTTP callback for preview image download
	CCallResult<CCFWorkshopItemDetails, HTTPRequestCompleted_t> m_callbackHTTPPreview;
	void Steam_OnPreviewImageReceived(HTTPRequestCompleted_t* pResult, bool bError);
	
	CCFWorkshopItem* m_pCurrentItem;
	EditablePanel* m_pContainer;  // The .res container with the controls
	bool m_bPendingSubscribe;
	bool m_bPendingUnsubscribe;
	HTTPRequestHandle m_hPendingPreviewRequest;
	CWorkshopPreviewImage* m_pPreviewImageData;  // Our custom preview image
	PublishedFileId_t m_nPreviewFileID;  // Track which item this preview is for
	
	Label* m_pTitleLabel;
	Label* m_pAuthorLabel;
	Label* m_pDescriptionLabel;
	Label* m_pFileSizeLabel;
	Label* m_pUpdatedLabel;
	Label* m_pTagsLabel;
	ImagePanel* m_pPreviewImage;
	Button* m_pSubscribeButton;
	Button* m_pUnsubscribeButton;
	Button* m_pOpenWorkshopButton;
	CheckButton* m_pEnableCheckbox;
};

//-----------------------------------------------------------------------------
// Main workshop browser panel - TF2 style
//-----------------------------------------------------------------------------
class CCFWorkshopBrowserPanel : public Frame
{
	DECLARE_CLASS_SIMPLE(CCFWorkshopBrowserPanel, Frame);

public:
	CCFWorkshopBrowserPanel(Panel* parent, const char* panelName);
	virtual ~CCFWorkshopBrowserPanel();

	virtual void ApplySchemeSettings(IScheme* pScheme) OVERRIDE;
	virtual void PerformLayout() OVERRIDE;
	virtual void OnCommand(const char* command) OVERRIDE;
	virtual void OnThink() OVERRIDE;

	// Show/hide panel
	void ShowPanel(bool bShow);

	// Refresh the workshop item list
	void RefreshList();

	// Handle item selection - ListPanel sends "ItemSelected" without itemID
	MESSAGE_FUNC(OnItemSelected, "ItemSelected");
	
	// Handle right-click context menu
	MESSAGE_FUNC_INT(OnOpenContextMenu, "OpenContextMenu", itemID);
	
	// Handle ComboBox text changes (when user selects a filter)
	MESSAGE_FUNC_PARAMS(OnTextChanged, "TextChanged", data);

private:
	// Context menu handling
	void ShowContextMenu(int itemID);
	
	// UI elements
	ListPanel* m_pItemList;
	TextEntry* m_pSearchBox;
	ComboBox* m_pFilterCombo;
	Button* m_pSearchButton;
	Button* m_pRefreshButton;
	Button* m_pMyItemsButton;
	Button* m_pUploadButton;
	Button* m_pCosmeticToolButton;
	Button* m_pWeaponToolButton;
	Button* m_pCloseButton;
	CCFWorkshopItemDetails* m_pDetailsPanel;

	// Context menu
	DHANDLE<Menu> m_hContextMenu;
	PublishedFileId_t m_nContextMenuFileID;

	// Current filter
	CFWorkshopItemType_t m_eCurrentFilter;
	bool m_bShowSubscribedOnly;
	char m_szCurrentTagFilter[256]; // Current tag filter string
	bool m_bFilterTagsDirty; // True if filter tags need to be repopulated
	uint32 m_nLastItemCount; // Track item count to detect when new items arrive
	
	// Update timer
	float m_flNextUpdateTime;

	// Helper functions
	void PopulateItemList();
	void ApplyFilters();
	void UpdateItemRow(int itemID, CCFWorkshopItem* pItem);
	void PopulateFilterTags();
};

//-----------------------------------------------------------------------------
// Workshop upload dialog - TF2 style
//-----------------------------------------------------------------------------
class CCFWorkshopUploadDialog : public Frame
{
	DECLARE_CLASS_SIMPLE(CCFWorkshopUploadDialog, Frame);

public:
	CCFWorkshopUploadDialog(Panel* parent, const char* panelName);
	virtual ~CCFWorkshopUploadDialog();

	virtual void ApplySchemeSettings(IScheme* pScheme) OVERRIDE;
	virtual void PerformLayout() OVERRIDE;
	virtual void OnCommand(const char* command) OVERRIDE;
	virtual void OnThink() OVERRIDE;

	// Set up for creating a new item
	void SetupForNewItem(CFWorkshopItemType_t type);
	
	// Set up for updating an existing item
	void SetupForUpdate(PublishedFileId_t fileID);
	
	// File dialog result handlers
	MESSAGE_FUNC_PARAMS(OnFileSelected, "FileSelected", params);

private:
	void DoUpload();
	void BuildTagString(char* pszOut, int maxLen);
	void OpenContentFolderBrowser();
	void OpenPreviewFileBrowser();
	void OpenScreenshotsFolderBrowser();

	PublishedFileId_t m_nFileID;
	CFWorkshopItemType_t m_eItemType;
	bool m_bIsUpdate;
	enum BrowsingType_t { BROWSE_CONTENT, BROWSE_PREVIEW, BROWSE_SCREENSHOTS };
	BrowsingType_t m_eBrowsingType;

	TextEntry* m_pTitleEntry;
	TextEntry* m_pDescriptionEntry;
	TextEntry* m_pTagsEntry;
	TextEntry* m_pContentPathEntry;
	TextEntry* m_pPreviewImageEntry;
	TextEntry* m_pScreenshotsFolderEntry;
	Button* m_pBrowseContentButton;
	Button* m_pBrowsePreviewButton;
	Button* m_pBrowseScreenshotsButton;
	Button* m_pSubmitButton;
	Button* m_pCancelButton;
	ComboBox* m_pTypeCombo;
	ComboBox* m_pVisibilityCombo;
	Label* m_pStatusLabel;
	ProgressBar* m_pProgressBar;
	ImagePanel* m_pPreviewImagePanel;
	CWorkshopPreviewImage* m_pPreviewImage;
	
	// Tag checkboxes - indexed by CFWorkshopTag_t enum
	CheckButton* m_pTagChecks[CF_WORKSHOP_TAG_COUNT];
};

//-----------------------------------------------------------------------------
// Workshop metadata update dialog - for editing existing items
//-----------------------------------------------------------------------------
class CCFWorkshopUpdateDialog : public Frame
{
	DECLARE_CLASS_SIMPLE(CCFWorkshopUpdateDialog, Frame);

public:
	CCFWorkshopUpdateDialog(Panel* parent, const char* panelName);
	virtual ~CCFWorkshopUpdateDialog();

	virtual void ApplySchemeSettings(IScheme* pScheme) OVERRIDE;
	virtual void OnCommand(const char* command) OVERRIDE;
	virtual void OnThink() OVERRIDE;

	// Set up for updating an existing item
	void SetupForItem(PublishedFileId_t fileID);

private:
	void DoUpdate();
	void BuildTagString(char* pszOut, int maxLen);

	PublishedFileId_t m_nFileID;

	TextEntry* m_pTitleEntry;
	TextEntry* m_pDescriptionEntry;
	ComboBox* m_pVisibilityCombo;
	Button* m_pSubmitButton;
	Button* m_pCancelButton;
	Label* m_pStatusLabel;
	ProgressBar* m_pProgressBar;
	
	// Tag checkboxes - indexed by CFWorkshopTag_t enum
	CheckButton* m_pTagChecks[CF_WORKSHOP_TAG_COUNT];
};

//-----------------------------------------------------------------------------
// Workshop delete confirmation dialog
//-----------------------------------------------------------------------------
class CCFWorkshopDeleteConfirmDialog : public Frame
{
	DECLARE_CLASS_SIMPLE(CCFWorkshopDeleteConfirmDialog, Frame);

public:
	CCFWorkshopDeleteConfirmDialog(Panel* parent, const char* panelName);
	virtual ~CCFWorkshopDeleteConfirmDialog();

	virtual void ApplySchemeSettings(IScheme* pScheme) OVERRIDE;
	virtual void OnCommand(const char* command) OVERRIDE;
	virtual void OnThink() OVERRIDE;

	// Set up for deleting an item
	void SetupForItem(PublishedFileId_t fileID, const char* pszTitle);

private:
	PublishedFileId_t m_nFileID;
	Label* m_pMessageLabel;
	Label* m_pStatusLabel;
	Button* m_pDeleteButton;
	Button* m_pCancelButton;
	bool m_bDeleteCompleted;
	float m_flCloseTime;
};

} // namespace vgui

#endif // CF_WORKSHOP_PANEL_H
