// Copyright (c) 2006-2012 Audiokinetic Inc. / All Rights Reserved

/*------------------------------------------------------------------------------------
	AkAudioBankGenerationHelpers.h: Wwise Helpers to generate banks from the editor and when cooking.
------------------------------------------------------------------------------------*/
#pragma once

#include "AkAudioClasses.h"

namespace WwiseBnkGenHelper
{
	/**
	 * Dump the bank definition to file
	 *
	 * @param in_DefinitionFileContent	Banks to include in file
	 */
	AUDIOKINETICTOOLS_API bool GenerateDefinitionFile(TArray< TSharedPtr<FString> >& BanksToGenerate, TMap<FString, TSet<UAkAudioEvent*> >& BankToEventSet);
	FString DumpBankContentString(TMap<FString, TSet<UAkAudioEvent*> >& in_DefinitionFileContent);
	FString DumpBankContentString(TMap<FString, TSet<UAkAuxBus*> >& in_DefinitionFileContent );
	
	/**
	 * Generate the Wwise soundbanks
	 *
	 * @param in_rBankNames				Names of the banks
	 * @param in_bImportDefinitionFile	Use an import definition file
	 */
	AUDIOKINETICTOOLS_API int32 GenerateSoundBanks( TArray< TSharedPtr<FString> >& in_rBankNames, TArray< TSharedPtr<FString> >& in_PlatformNames, const FString* WwisePathOverride = nullptr);

	AUDIOKINETICTOOLS_API void GetWwisePlatforms(TArray< TSharedPtr<FString> >& WwisePlatforms);
	void AddPlatformIfSupported(const TSet<FString>& SupportedPlatforms, const FString& UnrealName, const TCHAR* WwiseName, TArray< TSharedPtr<FString> >& WwisePlatforms);

	AUDIOKINETICTOOLS_API bool FetchAttenuationInfo(const TMap<FString, TSet<UAkAudioEvent*> >& BankToEventSet);

	void CreateGenerateSoundBankWindow(TArray<TWeakObjectPtr<UAkAudioBank>>* pSoundBanks, bool in_bShouldSaveWwiseProject);

	FString GetBankGenerationFullDirectory( const TCHAR * in_szPlatformDir );

	FString GetLinkedProjectPath();

	FString GetDefaultSBDefinitionFilePath();

	FString GetProjectDirectory();
};
