//====== Copyright Custom Fortress 2, All rights reserved. =================
//
// Workshop Weapon Mod Creation Tool
// Allows importing custom MDL files and mapping them to replace existing weapons
//
//=============================================================================

#ifndef CF_WORKSHOP_WEAPON_TOOL_H
#define CF_WORKSHOP_WEAPON_TOOL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/TextEntry.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/ListPanel.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/ImagePanel.h>
#include "basemodel_panel.h"

namespace vgui
{

// Weapon replacement entry
struct WeaponReplacementEntry_t
{
	CUtlString strCustomModelPath;
	CUtlString strTargetWeaponClass;
	CUtlString strTargetModelPath;
	CUtlString strDisplayName;
	bool bReplaceViewModel;
	bool bReplaceWorldModel;
};

// Model preview panel
class CWeaponModelPreviewPanel : public CBaseModelPanel
{
	DECLARE_CLASS_SIMPLE(CWeaponModelPreviewPanel, CBaseModelPanel);
public:
	CWeaponModelPreviewPanel(Panel* parent, const char* panelName);
	virtual ~CWeaponModelPreviewPanel();

	bool LoadModel(const char* pszModelPath);
	void ClearModel();
	void ResetCamera();
	void SetAutoRotate(bool bAutoRotate);
	bool IsAutoRotating() const { return m_bAutoRotate; }

	// Override SetMDL to safely handle model loading
	virtual void SetMDL(MDLHandle_t handle, void *pProxyData = NULL) OVERRIDE;
	virtual void SetMDL(const char *pMDLName, void *pProxyData = NULL) OVERRIDE;

	virtual void OnThink() OVERRIDE;
	virtual void ApplySchemeSettings(IScheme* pScheme) OVERRIDE;
	virtual void PaintBackground() OVERRIDE;

protected:
	virtual void OnMousePressed(MouseCode code) OVERRIDE;
	virtual void OnMouseReleased(MouseCode code) OVERRIDE;
	virtual void OnCursorMoved(int x, int y) OVERRIDE;
	virtual void OnMouseWheeled(int delta) OVERRIDE;

private:
	bool m_bAutoRotate;
	float m_flAutoRotateSpeed;
};

// Main Weapon Mod Tool Panel
class CCFWorkshopWeaponTool : public Frame
{
	DECLARE_CLASS_SIMPLE(CCFWorkshopWeaponTool, Frame);
public:
	CCFWorkshopWeaponTool(Panel* parent, const char* panelName);
	virtual ~CCFWorkshopWeaponTool();

	virtual void ApplySchemeSettings(IScheme* pScheme) OVERRIDE;
	virtual void PerformLayout() OVERRIDE;
	virtual void OnCommand(const char* command) OVERRIDE;
	virtual void OnThink() OVERRIDE;
	virtual void OnKeyCodePressed(KeyCode code) OVERRIDE;
	virtual void PostChildPaint() OVERRIDE;

	void ShowPanel(bool bShow);

	MESSAGE_FUNC_PARAMS(OnFileSelected, "FileSelected", params);
	MESSAGE_FUNC_PARAMS(OnDirectorySelected, "DirectorySelected", params);
	MESSAGE_FUNC(OnItemSelected, "ItemSelected");
	MESSAGE_FUNC_INT(OnItemDoubleClicked, "ItemDoubleClicked", itemID);

private:
	void OpenModelFileBrowser();
	void OpenOutputDirectoryBrowser();
	void AddModelReplacement();
	void RemoveSelectedReplacement();
	void ClearAllReplacements();
	void UpdateModelPreview();
	void PopulateWeaponDropdown();
	void FilterWeaponsByClass(int iClassIndex);
	void FilterWeaponsBySlot(int iSlot);
	void FilterWeaponsBySearch();
	void UpdateWeaponIcon();
	void GenerateWeaponMod();
	bool CopyModelToTarget(const char* pszSourcePath, const char* pszTargetModelPath);
	bool CopyModelFiles(const char* pszSourcePath, const char* pszDestPath);
	bool GenerateWeaponScript(const char* pszOutputPath);
	bool ValidateModelFile(const char* pszModelPath);
	void SetStatus(const char* pszFormat, ...);

	MESSAGE_FUNC_PARAMS(OnTextChanged, "TextChanged", data);

	enum BrowseType_t { BROWSE_MODEL, BROWSE_PREVIEW, BROWSE_OUTPUT };
	BrowseType_t m_eBrowseType;

	// UI Elements
	TextEntry* m_pModelPathEntry;
	Button* m_pBrowseModelButton;
	CWeaponModelPreviewPanel* m_pModelPreview;
	Label* m_pModelInfoLabel;
	CheckButton* m_pAutoRotateCheck;
	
	// Weapon selection controls
	TextEntry* m_pWeaponSearchEntry;
	ComboBox* m_pSlotFilterCombo;
	ComboBox* m_pClassFilterCombo;
	ComboBox* m_pWeaponCombo;
	Panel* m_pWeaponIconPanel;
	ImagePanel* m_pWeaponIconImage;
	int m_iWeaponIconTextureID;
	IMaterial* m_pWeaponIconMaterial;
	
	CheckButton* m_pReplaceViewModelCheck;
	CheckButton* m_pReplaceWorldModelCheck;
	Button* m_pAddReplacementButton;
	Button* m_pRemoveReplacementButton;
	ListPanel* m_pReplacementList;
	TextEntry* m_pModNameEntry;
	TextEntry* m_pModDescriptionEntry;
	TextEntry* m_pOutputPathEntry;
	Button* m_pBrowseOutputButton;
	Button* m_pGenerateButton;
	Button* m_pCancelButton;
	Label* m_pStatusLabel;
	
	CUtlVector<WeaponReplacementEntry_t> m_vecReplacements;
	CUtlString m_strCurrentModelPath;
	CUtlString m_strOutputPath;
	CUtlString m_strSearchFilter;
	float m_flNextStatusClearTime;
};

} // namespace vgui

void OpenWorkshopWeaponTool();

#endif // CF_WORKSHOP_WEAPON_TOOL_H
