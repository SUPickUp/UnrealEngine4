// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "SettingsPrivatePCH.h"


#define LOCTEXT_NAMESPACE "FSettingsModule"


/**
 * Implements the Settings module.
 */
class FSettingsModule
	: public ISettingsModule
{
public:

	// ISettingsModule interface

	virtual void GetContainerNames( TArray<FName>& OutNames ) const override
	{
		ContainerNamesToContainers.GenerateKeyArray(OutNames);
	}

	virtual ISettingsContainerPtr GetContainer( const FName& ContainerName ) override
	{
		return ContainerNamesToContainers.FindRef(ContainerName);
	}

	virtual ISettingsSectionPtr RegisterSettings( const FName& ContainerName, const FName& CategoryName, const FName& SectionName, const FText& DisplayName, const FText& Description, const TWeakObjectPtr<UObject>& SettingsObject, const FSettingsSectionDelegates& Delegates ) override
	{
		return FindOrAddContainer(ContainerName)->AddSection(CategoryName, SectionName, DisplayName, Description, SettingsObject, Delegates);
	}

	virtual ISettingsSectionPtr RegisterSettings( const FName& ContainerName, const FName& CategoryName, const FName& SectionName, const FText& DisplayName, const FText& Description, const TSharedRef<SWidget>& CustomWidget, const FSettingsSectionDelegates& Delegates ) override
	{
		return FindOrAddContainer(ContainerName)->AddSection(CategoryName, SectionName, DisplayName, Description, CustomWidget, Delegates);
	}

	virtual void RegisterViewer( const FName& ContainerName, ISettingsViewer& SettingsViewer )
	{
		ContainerNamesToViewers.Add(ContainerName, &SettingsViewer);
	}

	virtual void ShowViewer( const FName& ContainerName, const FName& CategoryName, const FName& SectionName )
	{
		ISettingsViewer** Viewer = ContainerNamesToViewers.Find(ContainerName);

		if (Viewer != nullptr)
		{
			(*Viewer)->ShowSettings(CategoryName, SectionName);
		}
	}

	virtual void UnregisterViewer( const FName& ContainerName )
	{
		ContainerNamesToViewers.Remove(ContainerName);
	}

	virtual void UnregisterSettings( const FName& ContainerName, const FName& CategoryName, const FName& SectionName ) override
	{
		TSharedPtr<FSettingsContainer> Container = ContainerNamesToContainers.FindRef(ContainerName);

		if (Container.IsValid())
		{
			Container->RemoveSection(CategoryName, SectionName);
		}
	}

public:

	// IModuleInterface interface
	
	virtual void StartupModule( ) override
	{
		// @todo gmp: move this into the modules that own these setting categories
		TSharedRef<FSettingsContainer> EditorSettingsContainer = FindOrAddContainer("Editor");
		EditorSettingsContainer->Describe(LOCTEXT("EditorPreferencesSubMenuLabel", "Editor Preferences"), LOCTEXT("EditorPreferencesSubMenuToolTip", "Configure the behavior and features of this Editor"), NAME_None);
		EditorSettingsContainer->DescribeCategory("General", LOCTEXT("EditorGeneralCategoryName", "General"), LOCTEXT("EditorGeneralCategoryDescription", "General Editor settings description text here"));
		EditorSettingsContainer->DescribeCategory("LevelEditor", LOCTEXT("EditorLevelEditorCategoryName", "Level Editor"), LOCTEXT("EditorLevelEditorCategoryDescription", "Level Editor settings description text here"));
		EditorSettingsContainer->DescribeCategory("ContentEditors", LOCTEXT("EditorContentEditorsCategoryName", "Content Editors"), LOCTEXT("EditorContentEditorsCategoryDescription", "Content editors settings description text here"));
		EditorSettingsContainer->DescribeCategory("Plugins", LOCTEXT("EditorPluginsCategoryName", "Plugins"), LOCTEXT("EditorPluginsCategoryDescription", "Plugins settings description text here"));

		// @todo gmp: move this into the modules that own these setting categories
		TSharedRef<FSettingsContainer> ProjectSettingsContainer = FindOrAddContainer("Project");
		ProjectSettingsContainer->Describe(LOCTEXT("ProjectSettingsSubMenuLabel", "Project Settings"), LOCTEXT("ProjectSettingsSubMenuToolTip", "Change the settings of the currently loaded project"), NAME_None);
		ProjectSettingsContainer->DescribeCategory("Project", LOCTEXT("ProjectProjectCategoryName", "Project"), LOCTEXT("ProjectProjectCategoryDescription", "Project settings description text here"));
		ProjectSettingsContainer->DescribeCategory("Engine", LOCTEXT("ProjectEngineCategoryName", "Engine"), LOCTEXT("ProjectEngineCategoryDescription", "Project settings description text here"));
		ProjectSettingsContainer->DescribeCategory("Platforms", LOCTEXT("ProjectPlatformsCategoryName", "Platforms"), LOCTEXT("ProjectPlatformsCategoryDescription", "Platform settings description text here"));
		ProjectSettingsContainer->DescribeCategory("Plugins", LOCTEXT("ProjectPluginsCategoryName", "Plugins"), LOCTEXT("ProjectPluginsCategoryDescription", "Plugins settings description text here"));
	}

	virtual void ShutdownModule( ) override { }

protected:

	/**
	 * Finds or adds the specified settings container.
	 *
	 * @param ContainerName The name of the container to find or add.
	 * @return The container.
	 */
	TSharedRef<FSettingsContainer> FindOrAddContainer( const FName& ContainerName )
	{
		TSharedPtr<FSettingsContainer>& Container = ContainerNamesToContainers.FindOrAdd(ContainerName);

		if (!Container.IsValid())
		{
			Container = MakeShareable(new FSettingsContainer(ContainerName));
		}

		return Container.ToSharedRef();
	}

private:

	// Holds the default settings container.
	ISettingsContainerPtr DefaultSettingsContainer;

	// Holds the collection of global settings containers.
	TMap<FName, TSharedPtr<FSettingsContainer> > ContainerNamesToContainers;

	// Holds the collection of registered settings viewers.
	TMap<FName, ISettingsViewer*> ContainerNamesToViewers;
};


#undef LOCTEXT_NAMESPACE


IMPLEMENT_MODULE(FSettingsModule, Settings);
