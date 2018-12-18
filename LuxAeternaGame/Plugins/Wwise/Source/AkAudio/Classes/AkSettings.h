#pragma once

#include "Engine/EngineTypes.h"
#include "AkSettings.generated.h"

#define AK_MAX_AUX_PER_OBJ	4

DECLARE_EVENT(UAkSettings, AutoConnectChanged);

UCLASS(config = Game, defaultconfig)
class AKAUDIO_API UAkSettings : public UObject
{
	GENERATED_UCLASS_BODY()

	// The number of AkReverbVolumes that will be simultaneously applied to a sound source
	UPROPERTY(Config, EditAnywhere, Category="Ak Reverb Volume")
	uint8 MaxSimultaneousReverbVolumes = AK_MAX_AUX_PER_OBJ;

	// Wwise Project Path
	UPROPERTY(Config, EditAnywhere, Category="Installation", meta=(FilePathFilter="wproj", AbsolutePath))
	FFilePath WwiseProjectPath;

	// Where the Sound Bank will be generated in the Content Folder
	UPROPERTY(Config, EditAnywhere, Category = "Sound Bank", meta=(RelativeToGameContentDir))
	FDirectoryPath WwiseSoundBankFolder;

	UPROPERTY(Config, EditAnywhere, Category = "Installation")
	bool bAutoConnectToWAAPI = false;

	// Allow to distribute SoundEngine processing tasks across multiple threads. Requires Editor restart.
	UPROPERTY(Config, EditAnywhere, Category = "Advanced")
	bool bEnableMultiCoreRendering = false;

	// Default value for Occlusion Collision Channel when creating a new Ak Component.
	UPROPERTY(Config, EditAnywhere, Category = "Occlusion")
	TEnumAsByte<ECollisionChannel> DefaultOcclusionCollisionChannel = ECollisionChannel::ECC_Visibility;

	UPROPERTY(Config)
	FDirectoryPath WwiseWindowsInstallationPath_DEPRECATED;

	UPROPERTY(Config)
	FFilePath WwiseMacInstallationPath_DEPRECATED;

	static FString DefaultSoundBankFolder;

	virtual void PostInitProperties() override;

#if WITH_EDITOR
	void EnsureSoundBankPathIsInPackagingSettings() const;
#endif

protected:
#if WITH_EDITOR
	virtual void PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent ) override;
	virtual void PreEditChange(UProperty* PropertyAboutToChange) override;
#endif

private:
	FString PreviousWwiseProjectPath;

public:
	bool bRequestRefresh = false;
    mutable AutoConnectChanged OnAutoConnectChanged;
};

