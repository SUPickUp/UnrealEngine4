// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "SettingsEditorPrivatePCH.h"
#include "EngineAnalytics.h"
#include "AnalyticsEventAttribute.h"
#include "IAnalyticsProvider.h"

#define LOCTEXT_NAMESPACE "SSettingsEditor"


/* SSettingsEditor structors
 *****************************************************************************/

SSettingsEditor::~SSettingsEditor( )
{
	Model->OnSelectionChanged().RemoveAll(this);
	SettingsContainer->OnCategoryModified().RemoveAll(this);
	FCoreDelegates::OnCultureChanged.RemoveAll(this);
}


/* SSettingsEditor interface
 *****************************************************************************/

void SSettingsEditor::Construct( const FArguments& InArgs, const ISettingsEditorModelRef& InModel )
{
	Model = InModel;
	DefaultConfigCheckOutTimer = 0.0f;
	DefaultConfigCheckOutNeeded = false;
	DefaultConfigQueryInProgress = false;
	SettingsContainer = InModel->GetSettingsContainer();

	// initialize settings view
	FDetailsViewArgs DetailsViewArgs;
	{
		DetailsViewArgs.bAllowSearch = true;
		DetailsViewArgs.bHideSelectionTip = true;
		DetailsViewArgs.bLockable = false;
		DetailsViewArgs.bObjectsUseNameArea = false;
		DetailsViewArgs.bSearchInitialKeyFocus = true;
		DetailsViewArgs.bUpdatesFromSelection = false;
		DetailsViewArgs.NotifyHook = this;
		DetailsViewArgs.bShowOptions = true;
		DetailsViewArgs.bShowModifiedPropertiesOption = false;
	}

	SettingsView = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor").CreateDetailView(DetailsViewArgs);
	SettingsView->SetVisibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &SSettingsEditor::HandleSettingsViewVisibility)));
	SettingsView->SetIsPropertyEditingEnabledDelegate(FIsPropertyEditingEnabled::CreateSP(this, &SSettingsEditor::HandleSettingsViewEnabled));

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(16.0f, 0.0f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 16.0f)
					[
						// categories menu
						SNew(SScrollBox)

						+ SScrollBox::Slot()
							[
								SNew(SHorizontalBox)

								+ SHorizontalBox::Slot()
									.AutoWidth()
									[
										SAssignNew(CategoriesBox, SVerticalBox)
									]

								+ SHorizontalBox::Slot()
									.AutoWidth()
									[
										SNew(SSpacer)
											.Size(FVector2D(24.0f, 0.0f))
									]
							]
					]

				+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(FMargin(24.0f, 0.0f, 24.0f, 0.0f))
					[
						SNew(SSeparator)
							.Orientation(Orient_Vertical)
					]

				+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.Padding(0.0f, 16.0f)
					[
						SNew(SVerticalBox)
							.Visibility(this, &SSettingsEditor::HandleSettingsBoxVisibility)

						+ SVerticalBox::Slot()
							.AutoHeight()
							[
								// title and button bar
								SNew(SHorizontalBox)

								+ SHorizontalBox::Slot()
									.FillWidth(1.0f)
									[
										SNew(SVerticalBox)

										+ SVerticalBox::Slot()
											.AutoHeight()
											[
												// category title
												SNew(STextBlock)
													.Font(FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 18))
													.Text(this, &SSettingsEditor::HandleSettingsBoxTitleText)													
											]

										+ SVerticalBox::Slot()
											.AutoHeight()
											.Padding(0.0f, 8.0f, 0.0f, 0.0f)
											[
												// category description
												SNew(STextBlock)
													.ColorAndOpacity(FSlateColor::UseSubduedForeground())
													.Text(this, &SSettingsEditor::HandleSettingsBoxDescriptionText)
											]
									]

								+ SHorizontalBox::Slot()
									.AutoWidth()
									.HAlign(HAlign_Right)
									.VAlign(VAlign_Bottom)
									.Padding(16.0f, 0.0f, 0.0f, 0.0f)
									[
										// set as default button
										SNew(SButton)
											.IsEnabled(this, &SSettingsEditor::HandleSetAsDefaultButtonEnabled)
											.OnClicked(this, &SSettingsEditor::HandleSetAsDefaultButtonClicked)
											.Text(LOCTEXT("SaveDefaultsButtonText", "Set as Default"))
											.ToolTipText(LOCTEXT("SaveDefaultsButtonTooltip", "Save the values below as the new default settings"))
									]

								+ SHorizontalBox::Slot()
									.AutoWidth()
									.HAlign(HAlign_Right)
									.VAlign(VAlign_Bottom)
									.Padding(8.0f, 0.0f, 0.0f, 0.0f)
									[
										// export button
										SNew(SButton)
											.IsEnabled(this, &SSettingsEditor::HandleExportButtonEnabled)
											.OnClicked(this, &SSettingsEditor::HandleExportButtonClicked)
											.Text(LOCTEXT("ExportButtonText", "Export..."))
											.ToolTipText(LOCTEXT("ExportButtonTooltip", "Export these settings to a file on your computer"))
									]

								+ SHorizontalBox::Slot()
									.AutoWidth()
									.HAlign(HAlign_Right)
									.VAlign(VAlign_Bottom)
									.Padding(8.0f, 0.0f, 0.0f, 0.0f)
									[
										// import button
										SNew(SButton)
											.IsEnabled(this, &SSettingsEditor::HandleImportButtonEnabled)
											.OnClicked(this, &SSettingsEditor::HandleImportButtonClicked)
											.Text(LOCTEXT("ImportButtonText", "Import..."))
											.ToolTipText(LOCTEXT("ImportButtonTooltip", "Import these settings from a file on your computer"))
									]

								+ SHorizontalBox::Slot()
									.AutoWidth()
									.HAlign(HAlign_Right)
									.VAlign(VAlign_Bottom)
									.Padding(8.0f, 0.0f, 0.0f, 0.0f)
									[
										// reset defaults button
										SNew(SButton)
											.IsEnabled(this, &SSettingsEditor::HandleResetToDefaultsButtonEnabled)
											.OnClicked(this, &SSettingsEditor::HandleResetDefaultsButtonClicked)
											.Text(LOCTEXT("ResetDefaultsButtonText", "Reset to Defaults"))
											.ToolTipText(LOCTEXT("ResetDefaultsButtonTooltip", "Reset the settings below to their default values"))
									]
							]

						+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.0f, 16.0f, 0.0f, 16.0f)
							[
								// checkout notice
								SNew(SSettingsEditorCheckoutNotice)
									.OnCheckOutClicked(this, &SSettingsEditor::HandleCheckOutButtonClicked)
									.Visibility(this, &SSettingsEditor::HandleDefaultConfigNoticeVisibility)
									.Unlocked(this, &SSettingsEditor::HandleConfigNoticeUnlocked)
									.LookingForSourceControlState(this, &SSettingsEditor::HandleLookingForSourceControlState)
							]

						+ SVerticalBox::Slot()
							.FillHeight(1.0f)
							.Padding(0.0f, 12.0f, 0.0f, 0.0f)
							[
								// settings area
								SNew(SOverlay)

								+ SOverlay::Slot()
									[
										SettingsView.ToSharedRef()
									]

								+ SOverlay::Slot()
									.Expose(CustomWidgetSlot)
							]
					]
			]
	];

	FCoreDelegates::OnCultureChanged.AddSP(this, &SSettingsEditor::HandleCultureChanged);
	Model->OnSelectionChanged().AddSP(this, &SSettingsEditor::HandleModelSelectionChanged);
	SettingsContainer->OnCategoryModified().AddSP(this, &SSettingsEditor::HandleSettingsContainerCategoryModified);

	ReloadCategories();
}


/* SCompoundWidget interface
 *****************************************************************************/

void SSettingsEditor::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	// cache selected settings object's configuration file state
	DefaultConfigCheckOutTimer += InDeltaTime;

	if (DefaultConfigCheckOutTimer >= 1.0f)
	{
		TWeakObjectPtr<UObject> SettingsObject = GetSelectedSettingsObject();

		if (SettingsObject.IsValid() && SettingsObject->GetClass()->HasAnyClassFlags(CLASS_Config | CLASS_DefaultConfig))
		{
			CachedConfigFileName = GetDefaultConfigFilePath(SettingsObject);
			bool NewCheckOutNeeded = false;
			if(ISourceControlModule::Get().IsEnabled())
			{
				// note: calling QueueStatusUpdate often does not spam status updates as an internal timer prevents this
				ISourceControlModule::Get().QueueStatusUpdate(CachedConfigFileName);

				ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
				FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(CachedConfigFileName, EStateCacheUsage::Use);
				NewCheckOutNeeded = SourceControlState.IsValid() && SourceControlState->CanCheckout();
				DefaultConfigQueryInProgress = SourceControlState.IsValid() && SourceControlState->IsUnknown();
			}
			else
			{
				NewCheckOutNeeded = (FPaths::FileExists(CachedConfigFileName) && IFileManager::Get().IsReadOnly(*CachedConfigFileName));
			}

			// file has been checked in or reverted
			if ((NewCheckOutNeeded == true) && (DefaultConfigCheckOutNeeded == false))
			{
				SettingsObject->ReloadConfig();
			}

			DefaultConfigCheckOutNeeded = NewCheckOutNeeded;
		}
		else
		{
			DefaultConfigCheckOutNeeded = false;
		}

		DefaultConfigCheckOutTimer = 0.0f;
	}
}


/* FNotifyHook interface
 *****************************************************************************/

void SSettingsEditor::NotifyPostChange( const FPropertyChangedEvent& PropertyChangedEvent, class FEditPropertyChain* PropertyThatChanged )
{
	ISettingsSectionPtr SelectedSection = Model->GetSelectedSection();

	if (SelectedSection.IsValid() && (PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive))
	{
		RecordPreferenceChangedAnalytics( SelectedSection, PropertyChangedEvent );

		SelectedSection->Save();
	}
}


/* SSettingsEditor implementation
 *****************************************************************************/

bool SSettingsEditor::CheckOutDefaultConfigFile( )
{
	TWeakObjectPtr<UObject> SettingsObject = GetSelectedSettingsObject();

	if (!SettingsObject.IsValid())
	{
		return false;
	}

	FText ErrorMessage;

	// check out configuration file
	FString AbsoluteConfigFilePath = GetDefaultConfigFilePath(SettingsObject);
	TArray<FString> FilesToBeCheckedOut;
	FilesToBeCheckedOut.Add(AbsoluteConfigFilePath);

	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(AbsoluteConfigFilePath, EStateCacheUsage::ForceUpdate);

	if (SourceControlState.IsValid())
	{
		if (SourceControlState->IsDeleted())
		{
			ErrorMessage = LOCTEXT("ConfigFileMarkedForDeleteError", "Error: The configuration file is marked for deletion.");
		}
		else if (!SourceControlState->IsCurrent())
		{
			if (false)
			{
				if (SourceControlProvider.Execute(ISourceControlOperation::Create<FSync>(), FilesToBeCheckedOut) == ECommandResult::Succeeded)
				{
					SettingsObject->ReloadConfig();
				}
				else
				{
					ErrorMessage = LOCTEXT("FailedToSyncConfigFileError", "Error: Failed to sync the configuration file to head revision.");
				}
			}
		}
		else if (SourceControlState->CanCheckout() || SourceControlState->IsCheckedOutOther())
		{
			if (SourceControlProvider.Execute(ISourceControlOperation::Create<FCheckOut>(), FilesToBeCheckedOut) == ECommandResult::Failed)
			{
				ErrorMessage = LOCTEXT("FailedToCheckOutConfigFileError", "Error: Failed to check out the configuration file.");
			}
		}
	}

	// show errors, if any
	if (!ErrorMessage.IsEmpty())
	{
		FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage);

		return false;
	}

	return true;
}


FString SSettingsEditor::GetDefaultConfigFilePath( const TWeakObjectPtr<UObject>& SettingsObject ) const
{
	FString RelativeConfigFilePath = FString::Printf(TEXT("%sDefault%s.ini"), *FPaths::SourceConfigDir(), *SettingsObject->GetClass()->ClassConfigName.ToString());

	return FPaths::ConvertRelativePathToFull(RelativeConfigFilePath);
}


TWeakObjectPtr<UObject> SSettingsEditor::GetSelectedSettingsObject( ) const
{
	ISettingsSectionPtr SelectedSection = Model->GetSelectedSection();

	if (SelectedSection.IsValid())
	{
		return SelectedSection->GetSettingsObject();
	}

	return nullptr;
}


TSharedRef<SWidget> SSettingsEditor::MakeCategoryWidget( const ISettingsCategoryRef& Category )
{
	// create section widgets
	TSharedRef<SVerticalBox> SectionsBox = SNew(SVerticalBox);
	TArray<ISettingsSectionPtr> Sections;
	
	if (Category->GetSections(Sections) == 0)
	{
		return SNullWidget::NullWidget;
	}

	// sort the sections alphabetically
	struct FSectionSortPredicate
	{
		FORCEINLINE bool operator()( ISettingsSectionPtr A, ISettingsSectionPtr B ) const
		{
			if (!A.IsValid() && !B.IsValid())
			{
				return false;
			}

			if (A.IsValid() != B.IsValid())
			{
				return B.IsValid();
			}

			return (A->GetDisplayName().CompareTo(B->GetDisplayName()) < 0);
		}
	};

	Sections.Sort(FSectionSortPredicate());

	// list the sections
	for (const ISettingsSectionPtr Section : Sections)
	{
		SectionsBox->AddSlot()
			.HAlign(HAlign_Left)
			.Padding(0.0f, 10.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 4.0f, 0.0f)
					.VAlign(VAlign_Center)
					[
						SNew(SImage)
							.Image(FEditorStyle::Get().GetBrush("TreeArrow_Collapsed_Hovered"))
							.Visibility(this, &SSettingsEditor::HandleSectionLinkImageVisibility, Section)
					]

				+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(SHyperlink)
							.OnNavigate(this, &SSettingsEditor::HandleSectionLinkNavigate, Section)
							.Text(Section->GetDisplayName())
							.ToolTipText(Section->GetDescription())
					]
			];

		if (!Model->GetSelectedSection().IsValid())
		{
			Model->SelectSection(Section);
		}
	}

	// create category widget
	return SNew(SVerticalBox)

	+ SVerticalBox::Slot()
		.AutoHeight()
		[
			// category title
			SNew(STextBlock)
				.Font(FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 18))
				.Text(Category->GetDisplayName())
		]

	+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			// sections list
			SectionsBox
		];
}


bool SSettingsEditor::MakeDefaultConfigFileWritable( )
{
	TWeakObjectPtr<UObject> SettingsObject = GetSelectedSettingsObject();

	if (!SettingsObject.IsValid())
	{
		return false;
	}

	FString AbsoluteConfigFilePath = GetDefaultConfigFilePath(SettingsObject);

	return FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*AbsoluteConfigFilePath, false);
}


void SSettingsEditor::ReloadCategories( )
{
	CategoriesBox->ClearChildren();

	TArray<ISettingsCategoryPtr> Categories;
	SettingsContainer->GetCategories(Categories);

	for (const ISettingsCategoryPtr Category : Categories)
	{
		CategoriesBox->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 16.0f)
			[
				MakeCategoryWidget(Category.ToSharedRef())
			];
	}
}


void SSettingsEditor::ShowNotification( const FText& Text, SNotificationItem::ECompletionState CompletionState ) const
{
	FNotificationInfo Notification(Text);
	Notification.ExpireDuration = 3.f;
	Notification.bUseSuccessFailIcons = false;

	FSlateNotificationManager::Get().AddNotification(Notification)->SetCompletionState(CompletionState);
}


/* SSettingsEditor callbacks
 *****************************************************************************/

FReply SSettingsEditor::HandleCheckOutButtonClicked( )
{
	if (ISourceControlModule::Get().IsEnabled())
	{
		CheckOutDefaultConfigFile();
	}
	else
	{
		MakeDefaultConfigFileWritable();
	}

	return FReply::Handled();
}


void SSettingsEditor::HandleCultureChanged( )
{
	ReloadCategories();
}


bool SSettingsEditor::HandleConfigNoticeUnlocked( ) const
{
	return !DefaultConfigCheckOutNeeded && !DefaultConfigQueryInProgress;
}

bool SSettingsEditor::HandleLookingForSourceControlState() const
{
	return DefaultConfigQueryInProgress;
}

void SSettingsEditor::RecordPreferenceChangedAnalytics( ISettingsSectionPtr SelectedSection, const FPropertyChangedEvent& PropertyChangedEvent ) const
{
	UProperty* ChangedProperty = PropertyChangedEvent.MemberProperty;
	// submit analytics data
	if(FEngineAnalytics::IsAvailable() && ChangedProperty != nullptr)
	{
		TArray<FAnalyticsEventAttribute> EventAttributes;
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("PropertySection"), SelectedSection->GetDisplayName().ToString()));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("PropertyClass"), ChangedProperty->GetOwnerClass()->GetName()));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("PropertyName"), ChangedProperty->GetName()));

		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.PreferencesChanged"), EventAttributes);
	}
}

EVisibility SSettingsEditor::HandleDefaultConfigNoticeVisibility( ) const
{
	TWeakObjectPtr<UObject> SettingsObject = GetSelectedSettingsObject();

	if (SettingsObject.IsValid() && SettingsObject->GetClass()->HasAnyClassFlags(CLASS_DefaultConfig))
	{
		return EVisibility::Visible;
	}

	return EVisibility::Collapsed;	
}


FReply SSettingsEditor::HandleExportButtonClicked( )
{
	ISettingsSectionPtr SelectedSection = Model->GetSelectedSection();

	if (SelectedSection.IsValid())
	{
		if (LastExportDir.IsEmpty())
		{
			LastExportDir = FPaths::GetPath(GEditorUserSettingsIni);
		}

		FString DefaultFileName = FString::Printf(TEXT("%s Backup %s.ini"), *SelectedSection->GetDisplayName().ToString(), *FDateTime::Now().ToString(TEXT("%Y-%m-%d %H%M%S")));
		TArray<FString> OutFiles;

		TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
		void* ParentWindowHandle = (ParentWindow.IsValid() && ParentWindow->GetNativeWindow().IsValid()) ? ParentWindow->GetNativeWindow()->GetOSWindowHandle() : nullptr;

		if (FDesktopPlatformModule::Get()->SaveFileDialog(ParentWindowHandle, LOCTEXT("ExportSettingsDialogTitle", "Export settings...").ToString(), LastExportDir, DefaultFileName, TEXT("Config files (*.ini)|*.ini"), EFileDialogFlags::None, OutFiles))
		{
			if (SelectedSection->Export(OutFiles[0]))
			{
				ShowNotification(LOCTEXT("ExportSettingsSuccess", "Export settings succeeded"), SNotificationItem::CS_Success);
			}
			else
			{
				ShowNotification(LOCTEXT("ExportSettingsFailure", "Export settings failed"), SNotificationItem::CS_Fail);
			}
		}
	}

	return FReply::Handled();
}


bool SSettingsEditor::HandleExportButtonEnabled( ) const
{
	ISettingsSectionPtr SelectedSection = Model->GetSelectedSection();

	if (SelectedSection.IsValid())
	{
		return SelectedSection->CanExport();
	}

	return false;
}


FReply SSettingsEditor::HandleImportButtonClicked( )
{
	ISettingsSectionPtr SelectedSection = Model->GetSelectedSection();

	if (SelectedSection.IsValid())
	{
		TArray<FString> OutFiles;

		TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
		void* ParentWindowHandle = (ParentWindow.IsValid() && ParentWindow->GetNativeWindow().IsValid()) ? ParentWindow->GetNativeWindow()->GetOSWindowHandle() : nullptr;

		if (FDesktopPlatformModule::Get()->OpenFileDialog(ParentWindowHandle, LOCTEXT("ImportSettingsDialogTitle", "Import settings...").ToString(), FPaths::GetPath(GEditorUserSettingsIni), TEXT(""), TEXT("Config files (*.ini)|*.ini"), EFileDialogFlags::None, OutFiles))
		{
			if (SelectedSection->Import(OutFiles[0]) && SelectedSection->Save())
			{
				ShowNotification(LOCTEXT("ImportSettingsSuccess", "Import settings succeeded"), SNotificationItem::CS_Success);
			}
			else
			{
				ShowNotification(LOCTEXT("ImportSettingsFailure", "Import settings failed"), SNotificationItem::CS_Fail);
			}
		}
	}

	return FReply::Handled();
}


bool SSettingsEditor::HandleImportButtonEnabled( ) const
{
	ISettingsSectionPtr SelectedSection = Model->GetSelectedSection();

	if (SelectedSection.IsValid())
	{
		return (SelectedSection->CanEdit() && SelectedSection->CanImport() && !DefaultConfigCheckOutNeeded);
	}

	return false;
}


void SSettingsEditor::HandleModelSelectionChanged( )
{
	ISettingsSectionPtr SelectedSection = Model->GetSelectedSection();

	if (SelectedSection.IsValid())
	{
		TSharedPtr<SWidget> CustomWidget = SelectedSection->GetCustomWidget().Pin();

		// show settings widget
		if (CustomWidget.IsValid())
		{
			CustomWidgetSlot->AttachWidget( CustomWidget.ToSharedRef() );
		}
		else
		{
			CustomWidgetSlot->AttachWidget( SNullWidget::NullWidget );
		}

		SettingsView->SetObject(SelectedSection->GetSettingsObject().Get());

		// focus settings widget
		TSharedPtr<SWidget> FocusWidget;

		if (CustomWidget.IsValid())
		{
			FocusWidget = CustomWidget;
		}
		else
		{
			FocusWidget = SettingsView;
		}

		FWidgetPath FocusWidgetPath;

		if (FSlateApplication::Get().GeneratePathToWidgetUnchecked(FocusWidget.ToSharedRef(), FocusWidgetPath))
		{
			FSlateApplication::Get().SetKeyboardFocus(FocusWidgetPath, EKeyboardFocusCause::SetDirectly);
		}
	}
	else
	{
		CustomWidgetSlot->AttachWidget( SNullWidget::NullWidget );
		SettingsView->SetObject(nullptr);
	}

	DefaultConfigCheckOutTimer = 1.0f;
}


FReply SSettingsEditor::HandleResetDefaultsButtonClicked( )
{
	ISettingsSectionPtr SelectedSection = Model->GetSelectedSection();

	if (SelectedSection.IsValid())
	{
		SelectedSection->ResetDefaults();
	}

	return FReply::Handled();
}


bool SSettingsEditor::HandleResetToDefaultsButtonEnabled( ) const
{
	ISettingsSectionPtr SelectedSection = Model->GetSelectedSection();

	if (SelectedSection.IsValid())
	{
		return (SelectedSection->CanEdit() && SelectedSection->CanResetDefaults());
	}

	return false;
}


void SSettingsEditor::HandleSectionLinkNavigate( ISettingsSectionPtr Section )
{
	Model->SelectSection(Section);
}


EVisibility SSettingsEditor::HandleSectionLinkImageVisibility( ISettingsSectionPtr Section ) const
{
	if (Model->GetSelectedSection() == Section)
	{
		return EVisibility::Visible;
	}

	return EVisibility::Hidden;
}


FReply SSettingsEditor::HandleSetAsDefaultButtonClicked( )
{
	ISettingsSectionPtr SelectedSection = Model->GetSelectedSection();

	if (SelectedSection.IsValid())
	{
		if (DefaultConfigCheckOutNeeded)
		{
			if (ISourceControlModule::Get().IsEnabled())
			{
				if (FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("SaveAsDefaultNeedsCheckoutMessage", "The default configuration file for these settings is currently not checked out. Would you like to check it out from source control?")) != EAppReturnType::Yes)
				{
					return FReply::Handled();
				}

				CheckOutDefaultConfigFile();
			}
			else
			{
				if (FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("SaveAsDefaultsIsReadOnlyMessage", "The default configuration file for these settings is currently not writeable. Would you like to make it writable?")) != EAppReturnType::Yes)
				{
					return FReply::Handled();
				}

				MakeDefaultConfigFileWritable();
			}
		}

		SelectedSection->SaveDefaults();

		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("SaveAsDefaultsSucceededMessage", "The default configuration file for these settings was updated successfully."));
	}

	return FReply::Handled();
}


bool SSettingsEditor::HandleSetAsDefaultButtonEnabled( ) const
{
	ISettingsSectionPtr SelectedSection = Model->GetSelectedSection();

	if (SelectedSection.IsValid())
	{
		return SelectedSection->CanSaveDefaults();
	}

	return false;
}


FText SSettingsEditor::HandleSettingsBoxDescriptionText( ) const
{
	ISettingsSectionPtr SelectedSection = Model->GetSelectedSection();

	if (SelectedSection.IsValid())
	{
		return SelectedSection->GetDescription();
	}

	return FText::GetEmpty();
}


FString SSettingsEditor::HandleSettingsBoxTitleText( ) const
{
	ISettingsSectionPtr SelectedSection = Model->GetSelectedSection();

	if (SelectedSection.IsValid())
	{
		return SelectedSection->GetCategory().Pin()->GetDisplayName().ToString() + TEXT(" - ") + SelectedSection->GetDisplayName().ToString();
	}

	return FString();
}


EVisibility SSettingsEditor::HandleSettingsBoxVisibility( ) const
{
	ISettingsSectionPtr SelectedSection = Model->GetSelectedSection();

	if (SelectedSection.IsValid())
	{
		return EVisibility::Visible;
	}

	return EVisibility::Hidden;
}


void SSettingsEditor::HandleSettingsContainerCategoryModified( const FName& CategoryName )
{
	ReloadCategories();
}


bool SSettingsEditor::HandleSettingsViewEnabled( ) const
{
	ISettingsSectionPtr SelectedSection = Model->GetSelectedSection();

	return (SelectedSection.IsValid() && SelectedSection->CanEdit() && (!SelectedSection->HasDefaultSettingsObject() || (!DefaultConfigCheckOutNeeded && !DefaultConfigQueryInProgress)));
}


EVisibility SSettingsEditor::HandleSettingsViewVisibility( ) const
{
	ISettingsSectionPtr SelectedSection = Model->GetSelectedSection();

	if (SelectedSection.IsValid() && SelectedSection->GetSettingsObject().IsValid())
	{
		return EVisibility::Visible;
	}

	return EVisibility::Hidden;
}


#undef LOCTEXT_NAMESPACE
