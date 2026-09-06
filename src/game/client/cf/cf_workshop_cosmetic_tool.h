//====== Copyright Custom Fortress 2, All rights reserved. =================
//
// Workshop Cosmetic Preview Tool
// Allows previewing cosmetic MDL files on different mercenary classes
// and generating cosmetic replacement mods
//
//=============================================================================

#ifndef CF_WORKSHOP_COSMETIC_TOOL_H
#define CF_WORKSHOP_COSMETIC_TOOL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/TextEntry.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/Slider.h>
#include <vgui_controls/ListPanel.h>
#include <vgui_controls/ImagePanel.h>
#include "tf/vgui/tf_playermodelpanel.h"

namespace vgui
{

// Cosmetic preview panel - extends CTFPlayerModelPanel with mouse manipulation enabled
class CCosmeticPreviewPanel : public CTFPlayerModelPanel
{
	DECLARE_CLASS_SIMPLE(CCosmeticPreviewPanel, CTFPlayerModelPanel);
public:
	CCosmeticPreviewPanel(Panel* parent, const char* panelName);
	virtual ~CCosmeticPreviewPanel();

	// Cosmetic model loading
	bool LoadCosmeticModel(const char* pszModelPath);
	void ClearCosmeticModel();
	const char* GetCosmeticPath() const { return m_strCosmeticModelPath.Get(); }
	
	// Class and team
	void SetPreviewClass(int iClass);
	void SetPreviewTeam(int iTeam);
	
	// Camera controls
	void ResetCamera();
	
	// Auto-rotate
	void SetAutoRotate(bool bAutoRotate);
	bool IsAutoRotating() const { return m_bAutoRotate; }
	
	// Animation control
	void SetAnimationSequence(int iSequence);
	void SetAnimationPlaybackRate(float flRate);
	int GetSequenceCount();
	const char* GetSequenceName(int iSequence);
	
	// Bodygroup control
	int GetBodygroupCount();
	const char* GetBodygroupName(int iGroup);
	int GetBodygroupValue(int iGroup);
	void SetBodygroupValue(int iGroup, int iValue);
	int GetBodygroupNumModels(int iGroup);

	virtual void OnThink() OVERRIDE;
	virtual void OnTick() OVERRIDE;
	virtual void ApplySchemeSettings(IScheme* pScheme) OVERRIDE;
	virtual void PaintBackground() OVERRIDE;

protected:
	virtual void OnMousePressed(MouseCode code) OVERRIDE;
	virtual void OnMouseReleased(MouseCode code) OVERRIDE;
	virtual void OnCursorMoved(int x, int y) OVERRIDE;
	virtual void OnMouseWheeled(int delta) OVERRIDE;

private:
	void SetupDefaultWeaponPose();
	void MountCosmeticSearchPath(const char* pszModelPath);
	void UnmountCosmeticSearchPath();

	CUtlString m_strCosmeticModelPath;
	CUtlString m_strMountedSearchPath;
	bool m_bAutoRotate;
	float m_flAutoRotateSpeed;
	bool m_bNeedsAnimationSetup;
	float m_flAnimationPlaybackRate;
	float m_flAnimationTime;
	float m_flLastTickTime;
};

// Cosmetic replacement entry for generating mods
struct CosmeticReplacementEntry_t
{
	CUtlString strCustomModelPath;
	CUtlString strTargetModelPath;
	CUtlString strDisplayName;
};

// Main Cosmetic Preview Tool Panel
class CCFWorkshopCosmeticTool : public Frame
{
	DECLARE_CLASS_SIMPLE(CCFWorkshopCosmeticTool, Frame);
public:
	CCFWorkshopCosmeticTool(Panel* parent, const char* panelName);
	virtual ~CCFWorkshopCosmeticTool();

	virtual void ApplySchemeSettings(IScheme* pScheme) OVERRIDE;
	virtual void PerformLayout() OVERRIDE;
	virtual void OnCommand(const char* command) OVERRIDE;
	virtual void OnThink() OVERRIDE;
	virtual void OnKeyCodePressed(KeyCode code) OVERRIDE;

	void ShowPanel(bool bShow);

	MESSAGE_FUNC_PARAMS(OnFileSelected, "FileSelected", params);
	MESSAGE_FUNC_PARAMS(OnDirectorySelected, "DirectorySelected", params);
	MESSAGE_FUNC_INT(OnSliderMoved, "SliderMoved", position);
	MESSAGE_FUNC_PARAMS(OnTextChanged, "TextChanged", params);

private:
	enum BrowseType_e
	{
		BROWSE_MODEL,
		BROWSE_TARGET_MODEL,
		BROWSE_OUTPUT
	};
	
	void OpenModelFileBrowser();
	void OpenTargetCosmeticBrowser();
	void OpenOutputDirectoryBrowser();
	void UpdatePreview();
	void PopulateClassDropdown();
	void PopulateSequenceDropdown();
	void PopulateBodygroupList();
	void ToggleSelectedBodygroup();
	void PopulateCosmeticDropdown();
	void FilterCosmeticsBySearch();
	void ApplyAllSettings();
	void AddCosmeticReplacement();
	void RemoveSelectedReplacement();
	void GenerateCosmeticMod();
	bool CopyModelToTarget(const char* pszSourcePath, const char* pszTargetModelPath);
	bool GenerateTF2Manifest();
	bool CreateZipFile(const char* pszZipPath, const char* pszSourceDir);
	void SetStatus(const char* pszFormat, ...);
	
	BrowseType_e m_eBrowseType;
	
	// UI Elements
	TextEntry* m_pModelPathEntry;
	Button* m_pBrowseModelButton;
	CCosmeticPreviewPanel* m_pCosmeticPreview;
	Label* m_pModelInfoLabel;
	
	// Class selection
	ComboBox* m_pClassCombo;
	ComboBox* m_pTeamCombo;
	
	// Animation controls
	ComboBox* m_pSequenceCombo;
	Slider* m_pAnimSpeedSlider;
	Label* m_pAnimSpeedLabel;
	
	// Bodygroup controls
	ListPanel* m_pBodygroupList;
	Button* m_pToggleBodygroupButton;
	
	// View controls
	CheckButton* m_pAutoRotateCheck;
	Button* m_pResetViewButton;
	
	// Apply button
	Button* m_pApplyButton;
	
	// Target cosmetic selection
	TextEntry* m_pCosmeticSearchEntry;
	ComboBox* m_pCosmeticSlotFilterCombo;
	ComboBox* m_pCosmeticCombo;
	Button* m_pBrowseTargetModelButton;
	Button* m_pAddReplacementButton;
	Button* m_pRemoveReplacementButton;
	ListPanel* m_pReplacementList;
	
	// Output directory
	TextEntry* m_pOutputPathEntry;
	Button* m_pBrowseOutputButton;
	CheckButton* m_pUseTF2FormatCheck;
	
	// Mod info
	TextEntry* m_pModNameEntry;
	TextEntry* m_pModDescriptionEntry;
	
	// Actions
	Button* m_pGenerateButton;
	Button* m_pCloseButton;
	Label* m_pStatusLabel;
	
	CUtlString m_strCurrentModelPath;
	CUtlString m_strTargetModelPath;
	CUtlString m_strOutputPath;
	CUtlVector<CosmeticReplacementEntry_t> m_vecReplacements;
	float m_flNextStatusClearTime;
	bool m_bNeedsSequenceDropdownRefresh;
	bool m_bModelInitialized;
};

} // namespace vgui

void OpenWorkshopCosmeticTool();

#endif // CF_WORKSHOP_COSMETIC_TOOL_H
