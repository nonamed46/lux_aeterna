// Copyright (c) 2006-2012 Audiokinetic Inc. / All Rights Reserved

/*------------------------------------------------------------------------------------
	AkAudioBankGenerationHelpers.cpp: Wwise Helpers to generate banks from the editor and when cooking.
------------------------------------------------------------------------------------*/

#include "AkAudioBankGenerationHelpers.h"
#include "AkAudioClasses.h"
#include "SGenerateSoundBanks.h"
#include "AkSettings.h"
#include "AssetRegistryModule.h"
#include "HAL/PlatformFileManager.h"
#include "Interfaces/IMainFrameModule.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Interfaces/ITargetPlatform.h"
#include "Misc/FileHelper.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Interfaces/IProjectManager.h"
#include "ProjectDescriptor.h"


#define LOCTEXT_NAMESPACE "AkAudio"

/** Whether we want the Cooking process to use Wwise to Re-generate banks.			*/
bool GIsWwiseCookingSoundBanks = true;

DEFINE_LOG_CATEGORY_STATIC(LogAk, Log, All);

FString WwiseBnkGenHelper::GetProjectDirectory()
{
#if UE_4_18_OR_LATER
	return FPaths::ProjectDir();
#else
	return FPaths::GameDir();
#endif // UE_4_18_OR_LATER
}

FString GetWwiseApplicationPath()
{
	const UAkSettingsPerUser* AkSettingsPerUser = GetDefault<UAkSettingsPerUser>();
	FString ApplicationToRun;
	ApplicationToRun.Empty();

	if( AkSettingsPerUser )
	{
#if PLATFORM_WINDOWS
		ApplicationToRun = AkSettingsPerUser->WwiseWindowsInstallationPath.Path;
#else
        ApplicationToRun = AkSettingsPerUser->WwiseMacInstallationPath.FilePath;
#endif
		if (FPaths::IsRelative(ApplicationToRun))
		{
			ApplicationToRun = FPaths::ConvertRelativePathToFull(WwiseBnkGenHelper::GetProjectDirectory(), ApplicationToRun);
		}
		if( !(ApplicationToRun.EndsWith(TEXT("/")) || ApplicationToRun.EndsWith(TEXT("\\"))) )
		{
			ApplicationToRun += TEXT("/");
		}

#if PLATFORM_WINDOWS
        if( FPaths::FileExists(ApplicationToRun + TEXT("Authoring/x64/Release/bin/WwiseCLI.exe")) )
		{
			ApplicationToRun += TEXT("Authoring/x64/Release/bin/WwiseCLI.exe");
		}
		else
		{
			ApplicationToRun += TEXT("Authoring/Win32/Release/bin/WwiseCLI.exe");
		}
        ApplicationToRun.ReplaceInline(TEXT("/"), TEXT("\\"));
#elif PLATFORM_MAC
        ApplicationToRun += TEXT("Contents/Tools/WwiseCLI.sh");
        ApplicationToRun = TEXT("\"") + ApplicationToRun + TEXT("\"");
#endif
	}

	return ApplicationToRun;
}

FString WwiseBnkGenHelper::GetLinkedProjectPath()
{
	// Get the Wwise Project Name from directory.
	const UAkSettings* AkSettings = GetDefault<UAkSettings>();
	FString ProjectPath;
	ProjectPath.Empty();
	
	if( AkSettings )
	{
		ProjectPath = AkSettings->WwiseProjectPath.FilePath;
	
		ProjectPath = FPaths::ConvertRelativePathToFull(WwiseBnkGenHelper::GetProjectDirectory(), ProjectPath);
#if PLATFORM_WINDOWS
		ProjectPath.ReplaceInline(TEXT("/"), TEXT("\\"));
#endif
	}

	return ProjectPath;
}

int32 RunWwiseBlockingProcess( const TCHAR* Parms, const FString* WwisePathOverride = nullptr )
{
    int32 ReturnCode = 0;
    
    // Starting the build in a separate process.
#if PLATFORM_WINDOWS
    FString wwiseCmd = GetWwiseApplicationPath();
    if (WwisePathOverride)
    {
        wwiseCmd = *WwisePathOverride;
    }
#else
    FString wwiseCmd("/bin/sh");
#endif

    UE_LOG(LogAk, Log, TEXT("Starting Wwise SoundBank generation with the following command line:"));
    UE_LOG(LogAk, Log, TEXT("%s %s"), *wwiseCmd, Parms);
    
    // Create a pipe for the child process's STDOUT.
    void* WritePipe;
    void* ReadPipe;
    FPlatformProcess::CreatePipe(ReadPipe, WritePipe);
    FProcHandle ProcHandle = FPlatformProcess::CreateProc( *wwiseCmd, Parms, false, false, false, nullptr, 0, nullptr, WritePipe );
    if( ProcHandle.IsValid() )
    {
        FString NewLine;
        FPlatformProcess::Sleep(0.1f);
        // Wait for it to finish and get return code
        while (FPlatformProcess::IsProcRunning(ProcHandle) == true)
        {
            NewLine = FPlatformProcess::ReadPipe(ReadPipe);
            if (NewLine.Len() > 0)
            {
                UE_LOG(LogAk, Display, TEXT("%s"), *NewLine);
                NewLine.Empty();
            }
            FPlatformProcess::Sleep(0.25f);
        }
        
        NewLine = FPlatformProcess::ReadPipe(ReadPipe);
        if (NewLine.Len() > 0)
        {
            UE_LOG(LogAk, Display, TEXT("%s"), *NewLine);
        }
        
        FPlatformProcess::GetProcReturnCode(ProcHandle, &ReturnCode);
        
        switch ( ReturnCode )
        {
            case 2:
                UE_LOG( LogAk, Warning, TEXT("Wwise command-line completed with warnings.") );
                break;
            case 0:
                UE_LOG( LogAk, Display, TEXT("Wwise command-line successfully completed.") );
                break; 
            default: 
                UE_LOG( LogAk, Error, TEXT("Wwise command-line failed with error %d."), ReturnCode );
                break; 
        }
    }
    else
    {
        ReturnCode = -1;
		// Most chances are the path to the .exe or the project were not set properly in GEditorIni file.
		UE_LOG( LogAk, Error, TEXT("Failed to run Wwise command-line: %s %s"), *wwiseCmd, Parms );
    } 
    
    FPlatformProcess::ClosePipe(ReadPipe, WritePipe);

    return ReturnCode;
}

FString WwiseBnkGenHelper::GetDefaultSBDefinitionFilePath()
{
	FString TempFileName = GetProjectDirectory();
	TempFileName += TEXT("TempDefinitionFile.txt");
	return TempFileName;
}

FString GetBankGenerationUserDirectory( const TCHAR * in_szPlatformDir );


bool WwiseBnkGenHelper::GenerateDefinitionFile(TArray< TSharedPtr<FString> >& BanksToGenerate, TMap<FString, TSet<UAkAudioEvent*> >& BankToEventSet)
{
	TMap<FString, TSet<UAkAuxBus*> > BankToAuxBusSet;
	{
		// Force load of event assets to make sure definition file is complete
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		TArray<FAssetData> EventAssets;
		AssetRegistryModule.Get().GetAssetsByClass(UAkAudioEvent::StaticClass()->GetFName(), EventAssets);

		for (int32 AssetIndex = 0; AssetIndex < EventAssets.Num(); ++AssetIndex)
		{
			FString EventAssetPath = EventAssets[AssetIndex].ObjectPath.ToString();
			UAkAudioEvent * pEvent = LoadObject<UAkAudioEvent>(NULL, *EventAssetPath, NULL, 0, NULL);
			if (BanksToGenerate.ContainsByPredicate([&](TSharedPtr<FString> Bank) {
				if (pEvent->RequiredBank)
				{
					return pEvent->RequiredBank->GetName() == *Bank;
				}
				return false;

			}))
			{
				if (pEvent->RequiredBank)
				{
					TSet<UAkAudioEvent*>& EventPtrSet = BankToEventSet.FindOrAdd(pEvent->RequiredBank->GetName());
					EventPtrSet.Add(pEvent);
				}
			}
		}

		// Force load of AuxBus assets to make sure definition file is complete
		TArray<FAssetData> AuxBusAssets;
		AssetRegistryModule.Get().GetAssetsByClass(UAkAuxBus::StaticClass()->GetFName(), AuxBusAssets);

		for (int32 AssetIndex = 0; AssetIndex < AuxBusAssets.Num(); ++AssetIndex)
		{
			FString AuxBusAssetPath = AuxBusAssets[AssetIndex].ObjectPath.ToString();
			UAkAuxBus * pAuxBus = LoadObject<UAkAuxBus>(NULL, *AuxBusAssetPath, NULL, 0, NULL);
			if (BanksToGenerate.ContainsByPredicate([&](TSharedPtr<FString> Bank) {
				if (pAuxBus->RequiredBank)
				{
					return pAuxBus->RequiredBank->GetName() == *Bank;
				}
				return false;
			}))
			{
				if (pAuxBus->RequiredBank)
				{
					TSet<UAkAuxBus*>& EventPtrSet = BankToAuxBusSet.FindOrAdd(pAuxBus->RequiredBank->GetName());
					EventPtrSet.Add(pAuxBus);
				}
			}
		}
	}

	FString DefFileContent = WwiseBnkGenHelper::DumpBankContentString(BankToEventSet);
	DefFileContent += WwiseBnkGenHelper::DumpBankContentString(BankToAuxBusSet);
	return FFileHelper::SaveStringToFile(DefFileContent, *WwiseBnkGenHelper::GetDefaultSBDefinitionFilePath());
}

/**
 * Dump the bank definition to file
 *
 * @param in_DefinitionFileContent	Banks to include in file
 */
FString WwiseBnkGenHelper::DumpBankContentString(TMap<FString, TSet<UAkAudioEvent*> >& in_DefinitionFileContent)
{
	// This generate the Bank definition file.
	// 
	FString FileContent;
	if (in_DefinitionFileContent.Num())
	{
		for (TMap<FString, TSet<UAkAudioEvent*> >::TIterator It(in_DefinitionFileContent); It; ++It)
		{
			if (It.Value().Num())
			{
				FString BankName = It.Key();
				for (TSet<UAkAudioEvent*>::TIterator ItEvent(It.Value()); ItEvent; ++ItEvent)
				{
					FString EventName = (*ItEvent)->GetName();
					FileContent += BankName + "\t\"" + EventName + "\"\n";
				}
			}
		}
	}

	return FileContent;
}

FString WwiseBnkGenHelper::DumpBankContentString(TMap<FString, TSet<UAkAuxBus*> >& in_DefinitionFileContent)
{
	// This generate the Bank definition file.
	// 
	FString FileContent;
	if (in_DefinitionFileContent.Num())
	{
		for (TMap<FString, TSet<UAkAuxBus*> >::TIterator It(in_DefinitionFileContent); It; ++It)
		{
			if (It.Value().Num())
			{
				FString BankName = It.Key();
				for (TSet<UAkAuxBus*>::TIterator ItAuxBus(It.Value()); ItAuxBus; ++ItAuxBus)
				{
					FString AuxBusName = (*ItAuxBus)->GetName();
					FileContent += BankName + "\t-AuxBus\t\"" + AuxBusName + "\"\n";
				}
			}
		}
	}

	return FileContent;
}

/**
 * Generate the Wwise soundbanks
 *
 * @param in_rBankNames				Names of the banks
 * @param in_bImportDefinitionFile	Use an import definition file
 */
int32 WwiseBnkGenHelper::GenerateSoundBanks( TArray< TSharedPtr<FString> >& in_rBankNames, TArray< TSharedPtr<FString> >& in_PlatformNames, const FString* WwisePathOverride/* = nullptr*/)
{
	long cNumBanks = in_rBankNames.Num();
	if( cNumBanks )
	{
		//////////////////////////////////////////////////////////////////////////////////////
		// For more information about how to generate banks using the command line, 
		// look in the Wwise SDK documentation 
		// in the section "Generating Banks from the Command Line"
		//////////////////////////////////////////////////////////////////////////////////////

        // Put the project name within quotes " ".
#if PLATFORM_WINDOWS
        FString CommandLineParams("");
#else
        FString CommandLineParams("");
        if(WwisePathOverride)
            CommandLineParams += *WwisePathOverride;
        else
            CommandLineParams += GetWwiseApplicationPath();
#endif

        CommandLineParams += FString::Printf( TEXT(" \"%s\""), *IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*GetLinkedProjectPath()) );
        
        // add the flag to request to generate sound banks if required.
		CommandLineParams += TEXT(" -GenerateSoundBanks");

		// For each bank to be generated, add " -Bank BankName"
		for ( long i = 0; i < cNumBanks; i++ )
		{
			CommandLineParams += FString::Printf(
				TEXT(" -Bank %s"),
				**in_rBankNames[i]
				);
		}

		// For each bank to be generated, add " -ImportDefinitionFile BankName"
        CommandLineParams += FString::Printf(
			TEXT(" -ImportDefinitionFile \"%s\""), 	
			*IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite( *GetDefaultSBDefinitionFilePath() )
			);

		// We could also specify the -Save flag.
		// It would cause the newly imported definition files to be persisted in the Wwise project files.
		// On the other hand, saving the project could cause the project currently being edited to be 
		// dirty if Wwise application is already running along with UnrealEditor, and the user would be 
		// prompted to either discard changes and reload the project, loosing local changes.
		// You can uncomment the following line if you want the Wwise project to be saved in this process.
		// By default, we prefer to not save it.
		// 
		// CommandLineParams += TEXT(" -Save");
		//

		// Generating for all asked platforms.
		if (in_PlatformNames.Num() == 0)
		{
			GetWwisePlatforms(in_PlatformNames);
		}

		for(int32 PlatformIdx = 0; PlatformIdx < in_PlatformNames.Num(); PlatformIdx++)
		{
            FString BankPath = WwiseBnkGenHelper::GetBankGenerationFullDirectory( **in_PlatformNames[PlatformIdx] );
#if PLATFORM_MAC
			// Workaround: This parameter does not work with Unix-style paths. convert it to Windows style.
            BankPath = FString(TEXT("Z:")) + BankPath;
#endif

            CommandLineParams += FString::Printf(
					TEXT(" -Platform %s -SoundBankPath %s \"%s\""), 
					**in_PlatformNames[PlatformIdx],
					**in_PlatformNames[PlatformIdx],
					*BankPath
					);
		}

		// Here you can specify languages, if no language is specified, all languages from the Wwise project.
		// will be built.
//#if PLATFORM_WINDOWS
//		CommandLineParams += TEXT(" -Language English(US)");
//#else
//		CommandLineParams += TEXT(" -Language English\\(US\\)");
//#endif

		// To get more information on how banks can be generated from the comand lines.
		// Refer to the section: Generating Banks from the Command Line in the Wwise SDK documentation.
		return RunWwiseBlockingProcess( *CommandLineParams, WwisePathOverride);
	}

	return -2;
}

FString WwiseBnkGenHelper::GetBankGenerationFullDirectory( const TCHAR * in_szPlatformDir )
{
	FString TargetDir = FPaths::ConvertRelativePathToFull(GetProjectDirectory());
	TargetDir += TEXT("Content\\WwiseAudio\\");
	TargetDir += in_szPlatformDir;

#if PLATFORM_WINDOWS
    TargetDir.ReplaceInline(TEXT("/"), TEXT("\\"));
#else
    TargetDir.ReplaceInline(TEXT("\\"), TEXT("/"));
#endif
	return TargetDir;
}

FString GetBankGenerationUserDirectory( const TCHAR * in_szPlatformDir )
{
	FString BankGenerationUserDirectory = FPaths::ConvertRelativePathToFull(WwiseBnkGenHelper::GetBankGenerationFullDirectory(in_szPlatformDir));
#if PLATFORM_WINDOWS
	BankGenerationUserDirectory.ReplaceInline(TEXT("/"), TEXT("\\"));
#endif
	return BankGenerationUserDirectory;
}

// Gets most recently modified JSON SoundBank metadata file
struct BankNameToPath : private IPlatformFile::FDirectoryVisitor
{
	FString BankPath;

	BankNameToPath(const FString& BankName, const TCHAR* BaseDirectory, IPlatformFile& PlatformFile)
		: BankName(BankName), PlatformFile(PlatformFile)
	{
		Visit(BaseDirectory, true);
		PlatformFile.IterateDirectory(BaseDirectory, *this);
	}

	bool IsValid() const { return StatData.bIsValid; }

private:
	virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
	{
		if (bIsDirectory)
		{
			FString NewBankPath = FilenameOrDirectory;
			NewBankPath += TEXT("\\");
			NewBankPath += BankName + TEXT(".json");

			FFileStatData NewStatData = PlatformFile.GetStatData(*NewBankPath);
			if (NewStatData.bIsValid && !NewStatData.bIsDirectory)
			{
				if (!StatData.bIsValid || NewStatData.ModificationTime > StatData.ModificationTime)
				{
					StatData = NewStatData;
					BankPath = NewBankPath;
				}
			}
		}

		return true;
	}

	const FString& BankName;
	IPlatformFile& PlatformFile;
	FFileStatData StatData;
};

void WwiseBnkGenHelper::AddPlatformIfSupported(const TSet<FString>& SupportedPlatforms, const FString& UnrealName, const TCHAR* WwiseName, TArray< TSharedPtr<FString> >& WwisePlatforms)
{
	if (SupportedPlatforms.Num() == 0 || SupportedPlatforms.Contains(UnrealName))
	{
		WwisePlatforms.Add(TSharedPtr<FString>(new FString(WwiseName)));
	}
}

void WwiseBnkGenHelper::GetWwisePlatforms(TArray< TSharedPtr<FString> >& WwisePlatforms)
{
	IProjectManager& ProjectManager = IProjectManager::Get();
	TSet<FString> SupportedPlatforms;
	for (FName TargetPlatform : ProjectManager.GetCurrentProject()->TargetPlatforms)
	{
		SupportedPlatforms.Add(TargetPlatform.ToString());
	}
	WwisePlatforms.Empty();
	AddPlatformIfSupported(SupportedPlatforms, TEXT("Android"), TEXT("Android"), WwisePlatforms);
	AddPlatformIfSupported(SupportedPlatforms, TEXT("IOS"), TEXT("IOS"), WwisePlatforms);
	AddPlatformIfSupported(SupportedPlatforms, TEXT("LinuxNoEditor"), TEXT("Linux"), WwisePlatforms);
#if UE_4_20_OR_LATER
	AddPlatformIfSupported(SupportedPlatforms, TEXT("Lumin"), TEXT("Lumin"), WwisePlatforms);
#endif
	AddPlatformIfSupported(SupportedPlatforms, TEXT("MacNoEditor"), TEXT("Mac"), WwisePlatforms);
	AddPlatformIfSupported(SupportedPlatforms, TEXT("PS4"), TEXT("PS4"), WwisePlatforms);
	AddPlatformIfSupported(SupportedPlatforms, TEXT("WindowsNoEditor"), TEXT("Windows"), WwisePlatforms);
	AddPlatformIfSupported(SupportedPlatforms, TEXT("XboxOne"), TEXT("XboxOne"), WwisePlatforms);
	AddPlatformIfSupported(SupportedPlatforms, TEXT("Switch"), TEXT("Switch"), WwisePlatforms);
}



bool WwiseBnkGenHelper::FetchAttenuationInfo(const TMap<FString, TSet<UAkAudioEvent*> >& BankToEventSet)
{
	FString PlatformName = GetTargetPlatformManagerRef().GetRunningTargetPlatform()->PlatformName();
	FString BankBasePath = WwiseBnkGenHelper::GetBankGenerationFullDirectory(*PlatformName);
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	const TCHAR* BaseDirectory = *BankBasePath;

	FString FileContents; // cache the file contents - in case we are opening large files

	for (TMap<FString, TSet<UAkAudioEvent*> >::TConstIterator BankIt(BankToEventSet); BankIt; ++BankIt)
	{
		FString BankName = BankIt.Key();
		BankNameToPath NameToPath(BankName, BaseDirectory, PlatformFile);

		if (NameToPath.IsValid())
		{
			const TCHAR* BankPath = *NameToPath.BankPath;
			const TSet<UAkAudioEvent*>& EventsInBank = BankIt.Value();

			FileContents.Reset();
			if (!FFileHelper::LoadFileToString(FileContents, BankPath))
			{
				UE_LOG(LogAk, Warning, TEXT("Failed to load file contents of JSON SoundBank metadata file: %s"), BankPath);
				continue;
			}

			TSharedPtr< FJsonObject > JsonObject;
			TSharedRef< TJsonReader<> > Reader = TJsonReaderFactory<>::Create(FileContents);

			if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
			{
				UE_LOG(LogAk, Warning, TEXT("Unable to parse JSON SoundBank metadata file: %s"), BankPath);
				continue;
			}

			TArray< TSharedPtr<FJsonValue> > SoundBanks = JsonObject->GetObjectField("SoundBanksInfo")->GetArrayField("SoundBanks");
			TSharedPtr<FJsonObject> Obj = SoundBanks[0]->AsObject();
			TArray< TSharedPtr<FJsonValue> > Events = Obj->GetArrayField("IncludedEvents");

			for (int i = 0; i < Events.Num(); i++)
			{
				TSharedPtr<FJsonObject> EventObj = Events[i]->AsObject();
				FString EventName = EventObj->GetStringField("Name");

				UAkAudioEvent* Event = nullptr;
				for (auto TestEvent : EventsInBank)
				{
					if (TestEvent->GetName() == EventName)
					{
						Event = TestEvent;
						break;
					}
				}

				if (Event == nullptr)
					continue;

				bool Changed = false;
				FString ValueString;
				if (EventObj->TryGetStringField("MaxAttenuation", ValueString))
				{
					const float EventRadius = FCString::Atof(*ValueString);
					if (Event->MaxAttenuationRadius != EventRadius)
					{
						Event->MaxAttenuationRadius = EventRadius;
						Changed = true;
					}
				}
				else
				{
					if (Event->MaxAttenuationRadius != 0)
					{
						// No attenuation info in json file, set to 0.
						Event->MaxAttenuationRadius = 0;
						Changed = true;
					}
				}

				// if we can't find "DurationType", then we assume infinite
				const bool IsInfinite = !EventObj->TryGetStringField("DurationType", ValueString) || (ValueString == "Infinite");
				if (Event->IsInfinite != IsInfinite)
				{
					Event->IsInfinite = IsInfinite;
					Changed = true;
				}

				if (!IsInfinite)
				{
					if (EventObj->TryGetStringField("DurationMin", ValueString))
					{
						const float DurationMin = FCString::Atof(*ValueString);
						if (Event->MinimumDuration != DurationMin)
						{
							Event->MinimumDuration = DurationMin;
							Changed = true;
						}
					}

					if (EventObj->TryGetStringField("DurationMax", ValueString))
					{
						const float DurationMax = FCString::Atof(*ValueString);
						if (Event->MaximumDuration != DurationMax)
						{
							Event->MaximumDuration = DurationMax;
							Changed = true;
						}
					}
				}

				if (Changed)
				{
					Event->Modify(true);
				}
			}
		}
	}

	return true;
}

/** Create the "Generate SoundBanks" window
 */
void WwiseBnkGenHelper::CreateGenerateSoundBankWindow(TArray<TWeakObjectPtr<UAkAudioBank>>* pSoundBanks, bool in_bShouldSaveWwiseProject)
{
	TSharedRef<SWindow> WidgetWindow =	SNew(SWindow)
		.Title( LOCTEXT("AkAudioGenerateSoundBanks", "Generate SoundBanks") )
		//.CenterOnScreen(true)
		.ClientSize(FVector2D(600.f, 332.f))
		.SupportsMaximize(false) .SupportsMinimize(false)
		.SizingRule( ESizingRule::FixedSize )
		.FocusWhenFirstShown(true);

	TSharedRef<SGenerateSoundBanks> WindowContent = SNew(SGenerateSoundBanks, pSoundBanks);
    WindowContent->SetShouldSaveWwiseProject(in_bShouldSaveWwiseProject);
	if (!WindowContent->ShouldDisplayWindow())
	{
		return;
	}

	// Add our SGenerateSoundBanks to the window
	WidgetWindow->SetContent( WindowContent );
	
	// Set focus to our SGenerateSoundBanks widget, so our keyboard keys work right off the bat
	WidgetWindow->SetWidgetToFocusOnActivate(WindowContent);

	// This creates a windows that blocks the rest of the UI. You can only interact with the "Generate SoundBanks" window.
	// If you choose to use this, comment the rest of the function.
	//GEditor->EditorAddModalWindow(WidgetWindow);

	// This creates a window that still allows you to interact with the rest of the editor. If there is an attempt to delete
	// a UAkAudioBank (from the content browser) while this window is opened, the editor will generate a (cryptic) error message
	TSharedPtr<SWindow> ParentWindow;
	if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
	{
		IMainFrameModule& MainFrame = FModuleManager::GetModuleChecked<IMainFrameModule>("MainFrame");
		ParentWindow = MainFrame.GetParentWindow();
	}

	if (ParentWindow.IsValid())
	{
		// Parent the window to the main frame 
		FSlateApplication::Get().AddWindowAsNativeChild(WidgetWindow, ParentWindow.ToSharedRef());
	}
	else
	{
		// Spawn new window
		FSlateApplication::Get().AddWindow(WidgetWindow);
	}
}

/**
 * Used as a delegate to create the menu section and entries for Audiokinetic item in the build menu
 */
void AddGenerateAkBanksToBuildMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.BeginSection("Audiokinetic", LOCTEXT("Audiokinetic", "Audiokinetic") );
	{
		FUIAction UIAction;

		UIAction.ExecuteAction.BindStatic(&WwiseBnkGenHelper::CreateGenerateSoundBankWindow, (TArray<TWeakObjectPtr<UAkAudioBank>>*)nullptr, false);
		MenuBuilder.AddMenuEntry(
			LOCTEXT("AkAudioBank_GenerateDefinitionFile","Generate SoundBanks..."),
			LOCTEXT("AkAudioBank_GenerateDefinitionFileTooltip", "Generates Wwise SoundBanks."),
			FSlateIcon(),
			UIAction
			);
	}
	MenuBuilder.EndSection();
}
 

#undef LOCTEXT_NAMESPACE
