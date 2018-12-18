// Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;
using System;
using System.IO;
using System.Collections.Generic;

public class AkAudio : ModuleRules
{
	string akLibPath = string.Empty;

	bool TargetPlatformIsNintendoSwitch = false;

#if WITH_FORWARDED_MODULE_RULES_CTOR
    private void AddWwiseLib(ReadOnlyTargetRules Target, string in_libName)
#else
	private void AddWwiseLib(TargetInfo Target, string in_libName)
#endif
	{
#if UE_4_20_OR_LATER
		if (Target.Platform == UnrealTargetPlatform.PS4 || Target.Platform == UnrealTargetPlatform.Android || Target.Platform == UnrealTargetPlatform.Lumin || Target.Platform == UnrealTargetPlatform.Linux || Target.Platform == UnrealTargetPlatform.IOS || TargetPlatformIsNintendoSwitch)
#else
		if (Target.Platform == UnrealTargetPlatform.PS4 || Target.Platform == UnrealTargetPlatform.Android || Target.Platform == UnrealTargetPlatform.Linux || Target.Platform == UnrealTargetPlatform.IOS || TargetPlatformIsNintendoSwitch)
#endif
		{
			PublicAdditionalLibraries.Add(in_libName);
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicAdditionalLibraries.Add(Path.Combine(akLibPath, "lib" + in_libName + ".a"));
		}
		else
		{
			PublicAdditionalLibraries.Add(in_libName + ".lib");
		}
	}

    private string GetPluginFullPath(string PluginName, string LibPath)
    {
#if UE_4_20_OR_LATER
        if (Target.Platform == UnrealTargetPlatform.Mac || Target.Platform == UnrealTargetPlatform.PS4 || Target.Platform == UnrealTargetPlatform.Android || Target.Platform == UnrealTargetPlatform.Lumin || Target.Platform == UnrealTargetPlatform.Linux || Target.Platform == UnrealTargetPlatform.IOS || TargetPlatformIsNintendoSwitch)
#else
        if (Target.Platform == UnrealTargetPlatform.Mac || Target.Platform == UnrealTargetPlatform.PS4 || Target.Platform == UnrealTargetPlatform.Android || Target.Platform == UnrealTargetPlatform.Linux || Target.Platform == UnrealTargetPlatform.IOS || TargetPlatformIsNintendoSwitch)
#endif
        {
            return Path.Combine(LibPath, "lib" + PluginName + ".a");
        }
        else
        {
            return Path.Combine(LibPath, PluginName + ".lib");
        }
    }

    private bool IsPluginInstalled(string PluginName)
    {
        foreach (string LibPath in PublicLibraryPaths)
        {
            if (File.Exists(GetPluginFullPath(PluginName, LibPath)))
            {
                return true;
            }
        }

        return false;
    }

#if WITH_FORWARDED_MODULE_RULES_CTOR
    private void AddWwisePlugin(ReadOnlyTargetRules Target, string in_libName)
#else
    private void AddWwisePlugin(TargetInfo Target, string in_libName)
#endif
    {
#if UE_4_19_OR_LATER
        var Defs = PublicDefinitions;
#else
        var Defs = Definitions;
#endif
        if (IsPluginInstalled(in_libName))
        {
            AddWwiseLib(Target, in_libName);
            Defs.Add("AK_WITH_" + in_libName.ToUpper() + "=1");

        }
        else
        {
            Defs.Add("AK_WITH_" + in_libName.ToUpper() + "=0");
        }
    }

#if UE_4_18_OR_LATER
    private string GetVisualStudioVersion(ReadOnlyTargetRules Target)
#else
    private string GetVisualStudioVersion()
#endif
    {
#if UE_4_18_OR_LATER
        WindowsCompiler Compiler = Target.WindowsPlatform.Compiler;
#else
        WindowsCompiler Compiler = WindowsPlatform.Compiler;
#endif
        string VSVersion = "vc140";

		try
		{
			if (Compiler == (WindowsCompiler)Enum.Parse(typeof(WindowsCompiler), "VisualStudio2013"))
			{
				VSVersion = "vc120";
			}
		}
		catch (Exception)
		{
		}

		try
		{
			if (Compiler == (WindowsCompiler)Enum.Parse(typeof(WindowsCompiler), "VisualStudio2015"))
			{
				VSVersion = "vc140";
			}
		}
		catch (Exception)
		{
		}

		try
		{
			if (Compiler == (WindowsCompiler)Enum.Parse(typeof(WindowsCompiler), "VisualStudio2017"))
			{
				VSVersion = "vc150";
			}
		}
		catch (Exception)
		{
		}

		return VSVersion;
	}

    private bool SupportsAkAutobahn()
    {
        if (Target.Configuration == UnrealTargetConfiguration.Shipping)
            return false;

		if(Target.Platform == UnrealTargetPlatform.Mac || Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64)
		    return true;
		
		return false;
    }
    
#if UE_4_17_OR_LATER
    public AkAudio(ReadOnlyTargetRules Target) : base(Target)
#else
	public AkAudio(TargetInfo Target)
#endif
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateIncludePathModuleNames.AddRange(new string[] { "Settings", "UMG"});
#if UE_4_17_OR_LATER
        ReadOnlyTargetRules BuildConfig = Target;
#else
        TargetRules BuildConfig = UEBuildConfiguration;
#endif
        // Need to use Enum.Parse because Switch is not present in UE < 4.15
        try
        {
			var SwitchPlatform = (UnrealTargetPlatform)Enum.Parse(typeof(UnrealTargetPlatform), "Switch");
			if (SwitchPlatform != UnrealTargetPlatform.Unknown)
				TargetPlatformIsNintendoSwitch = (Target.Platform == SwitchPlatform);
		}
		catch (Exception)
		{
		}

		PrivateIncludePaths.AddRange(
			new string[] {
			"AkAudio/Private",
			}
		);

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "SlateCore",
                "NetworkReplayStreaming",
				"MovieScene",
				"MovieSceneTracks",
                "Projects",
                "Json",
                "Slate",
                "InputCore"
            });

        PublicDependencyModuleNames.Add("UMG");
        
        PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"SlateCore",
				"NetworkReplayStreaming",
				"Projects",
			});

		string akDir = Path.GetFullPath(Path.Combine(ModuleDirectory, "../../ThirdParty"));
		List<string> akPlatformLibDir = new List<string>();

#if UE_4_19_OR_LATER
        var Defs = PublicDefinitions;
#else
        var Defs = Definitions;
#endif

        Defs.Add("USE_AKAUDIO");

        PublicIncludePaths.AddRange(
			new string[] {
				// SDK includes
				Path.Combine(akDir, "include"),
            }
        );

        // These definitions can be set as platform-specific
        if (BuildConfig.bBuildEditor == true)
        {
            // Boost the number of IO for the editor, since it uses the old IO system
            Defs.Add("AK_UNREAL_MAX_CONCURRENT_IO=256");
        }
        else
        {
            Defs.Add("AK_UNREAL_MAX_CONCURRENT_IO=32");
        }

        Defs.Add("AK_UNREAL_IO_GRANULARITY=32768");

        if (Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64)
		{
			string tempDir = (Target.Platform == UnrealTargetPlatform.Win32) ? "Win32_" : "x64_";
#if UE_4_18_OR_LATER
			tempDir += GetVisualStudioVersion(Target);
#else
            tempDir += GetVisualStudioVersion();
#endif
            akPlatformLibDir.Add(tempDir);
            string LibFolder = (Target.Platform == UnrealTargetPlatform.Win32) ? "x86" : "x64";
            PublicLibraryPaths.Add("$(DXSDK_DIR)" + Path.DirectorySeparatorChar + "Lib" + Path.DirectorySeparatorChar + LibFolder);

			if (BuildConfig.bBuildEditor == true)
            {
                // Sound frame is required for enabling communication between Wwise Application and the unreal editor.
                // Not to be defined in shipping mode.
                Defs.Add("AK_SOUNDFRAME");
            }

			PublicAdditionalLibraries.Add("dsound.lib");
			PublicAdditionalLibraries.Add("dxguid.lib");
			PublicAdditionalLibraries.Add("Msacm32.lib");
			PublicAdditionalLibraries.Add("XInput.lib");
            PublicAdditionalLibraries.Add("dinput8.lib");
        }
		else if (Target.Platform == UnrealTargetPlatform.XboxOne)
		{
			string VSVersion = "vc140";

			// Use reflection because the GitHub version of UE is missing things.
			var XboxOnePlatformType = System.Type.GetType("XboxOnePlatform", false);
			if (XboxOnePlatformType != null)
			{
				var XboxOneCompilerField = XboxOnePlatformType.GetField("Compiler");
				if (XboxOneCompilerField != null)
				{
					var XboxOneCompilerValue = XboxOneCompilerField.GetValue(null);
					if (XboxOneCompilerValue.ToString() == "VisualStudio2012")
						VSVersion = "vc110";
				}
			}

            akPlatformLibDir.Add("XboxOne_" + VSVersion);

            PublicAdditionalLibraries.Add("AcpHal.lib");
			PublicAdditionalLibraries.Add("MMDevApi.lib");
            Defs.Add("_XBOX_ONE");
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
            akPlatformLibDir.Add("Linux_x64");
        }
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
            akPlatformLibDir.Add("Mac");
        }
		else if (Target.Platform == UnrealTargetPlatform.IOS)
		{
            akPlatformLibDir.Add("iOS");
		}
		else if (Target.Platform == UnrealTargetPlatform.PS4)
		{
            akPlatformLibDir.Add("PS4");
            PublicAdditionalLibraries.Add("SceAjm_stub_weak");
			PublicAdditionalLibraries.Add("SceAudio3d_stub_weak");
            PublicAdditionalLibraries.Add("SceMove_stub_weak");
            Defs.Add("__ORBIS__");
        }
        else if (Target.Platform == UnrealTargetPlatform.Android)
		{
            akPlatformLibDir.Add("Android_armeabi-v7a");
            akPlatformLibDir.Add("Android_x86");
            akPlatformLibDir.Add("Android_arm64-v8a");
            akPlatformLibDir.Add("Android_x86_64");
            Defs.Add("__ANDROID__");
        }
		else if (TargetPlatformIsNintendoSwitch)
		{
            akPlatformLibDir.Add("NX64");
            Defs.Add("NN_NINTENDO_SDK");
        }
#if UE_4_20_OR_LATER
		else if (Target.Platform == UnrealTargetPlatform.Lumin)
		{
			akPlatformLibDir.Add("Lumin");
			Defs.Add("AK_LUMIN");
		}
#endif

		if (Target.Configuration == UnrealTargetConfiguration.Shipping)
		{
            Defs.Add("AK_OPTIMIZED");
        }

		string akConfigurationDir;

		if (Target.Configuration == UnrealTargetConfiguration.Debug)
		{
			// change bDebugBuildsActuallyUseDebugCRT to true in BuildConfiguration.cs to actually link debug binaries
#if UE_4_18_OR_LATER
            if (!Target.bDebugBuildsActuallyUseDebugCRT)
#else
            if (!BuildConfiguration.bDebugBuildsActuallyUseDebugCRT)
#endif
			{
				akConfigurationDir = "Profile";
			}
			else
			{
				akConfigurationDir = "Debug";
			}
		}
		else if (Target.Configuration == UnrealTargetConfiguration.Development ||
				Target.Configuration == UnrealTargetConfiguration.Test ||
				Target.Configuration == UnrealTargetConfiguration.DebugGame)
		{
			akConfigurationDir = "Profile";
		}
		else // if (Target.Configuration == UnrealTargetConfiguration.Shipping)
		{
			akConfigurationDir = "Release";
		}

		if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			akConfigurationDir += "-iphoneos";
		}

		if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			// No profiler support in the cross-compile toolchain.
			akConfigurationDir = "Release";
		}

        foreach (string libDir in akPlatformLibDir)
        {
            akLibPath = Path.Combine(Path.Combine(Path.Combine(akDir, libDir), akConfigurationDir), "lib");
            PublicLibraryPaths.Add(akLibPath);
        }

		AddWwiseLib(Target, "AkSoundEngine");
		AddWwiseLib(Target, "AkMemoryMgr");
		AddWwiseLib(Target, "AkStreamMgr");
		AddWwiseLib(Target, "AkMusicEngine");
		AddWwiseLib(Target, "AkSpatialAudio");

		AddWwiseLib(Target, "AkVorbisDecoder");
		AddWwiseLib(Target, "AkSilenceSource");
		AddWwiseLib(Target, "AkSineSource");
		AddWwiseLib(Target, "AkToneSource");
		AddWwiseLib(Target, "AkPeakLimiterFX");
		AddWwiseLib(Target, "AkMatrixReverbFX");
		AddWwiseLib(Target, "AkParametricEQFX");
		AddWwiseLib(Target, "AkDelayFX");
		AddWwiseLib(Target, "AkExpanderFX");
		AddWwiseLib(Target, "AkFlangerFX");
		AddWwiseLib(Target, "AkCompressorFX");
		AddWwiseLib(Target, "AkGainFX");
		AddWwiseLib(Target, "AkHarmonizerFX");
		AddWwiseLib(Target, "AkTimeStretchFX");
		AddWwiseLib(Target, "AkPitchShifterFX");
		AddWwiseLib(Target, "AkStereoDelayFX");
		AddWwiseLib(Target, "AkMeterFX");
		AddWwiseLib(Target, "AkGuitarDistortionFX");
		AddWwiseLib(Target, "AkTremoloFX");
		AddWwiseLib(Target, "AkRoomVerbFX");
		AddWwiseLib(Target, "AkAudioInputSource");
		AddWwiseLib(Target, "AkSynthOneSource");
		AddWwiseLib(Target, "AkRecorderFX");
        AddWwiseLib(Target, "AkMotionGeneratorSource");

        AddWwisePlugin(Target, "AkReflectFX");
        AddWwisePlugin(Target, "AkConvolutionReverbFX");
        AddWwisePlugin(Target, "AuroHeadphoneFX");
		AddWwisePlugin(Target, "AkMotionSink");

        if (SupportsAkAutobahn())
        {
            Defs.Add("AK_SUPPORT_WAAPI=1");
            AddWwiseLib(Target, "AkAutobahn");
        }
        else
        {
            Defs.Add("AK_SUPPORT_WAAPI=0");
        }

		if (Target.Platform == UnrealTargetPlatform.PS4)
		{
			AddWwiseLib(Target, "SceAudio3dEngine");
		}

		if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicAdditionalFrameworks.Add(new UEBuildFramework("AudioUnit"));
			PublicAdditionalFrameworks.Add(new UEBuildFramework("AudioToolbox"));
			PublicAdditionalFrameworks.Add(new UEBuildFramework("CoreAudio"));
			AddWwiseLib(Target, "AkAACDecoder");
		}

		if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			PublicAdditionalFrameworks.Add(new UEBuildFramework("AudioToolbox"));
			PublicAdditionalFrameworks.Add(new UEBuildFramework("CoreAudio"));
			AddWwiseLib(Target, "AkAACDecoder");
		}

		if (TargetPlatformIsNintendoSwitch)
		{
			AddWwiseLib(Target, "AkOpusNXDecoder");
		}
		else
		{
			AddWwiseLib(Target, "AkOpusDecoder");
		}

		if (Defs.Contains("AK_OPTIMIZED") == false && Target.Platform != UnrealTargetPlatform.Linux)
		{
			AddWwiseLib(Target, "CommunicationCentral");
		}

		// SoundFrame libs
		if (Defs.Contains("AK_SOUNDFRAME") == true)
		{
			PublicAdditionalLibraries.Add("SFLib.lib");
		}

		// If AK_SOUNDFRAME is defined, make UnrealEd a dependency
		if (BuildConfig.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.Add("SlateCore");
			PrivateDependencyModuleNames.Add("Slate");
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}
