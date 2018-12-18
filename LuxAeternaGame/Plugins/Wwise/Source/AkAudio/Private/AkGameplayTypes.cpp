// Copyright (c) 2006-2012 Audiokinetic Inc. / All Rights Reserved

/*=============================================================================
	AkAudioClasses.cpp:
=============================================================================*/

#include "AkGameplayTypes.h"
#include "AkAudioDevice.h"
#include "AkAudioClasses.h"
#include "EngineUtils.h"
#include "Engine/GameEngine.h"


UAkCallbackInfo* AkCallbackTypeHelpers::GetBlueprintableCallbackInfo(AkCallbackType CallbackType, AkCallbackInfo* CallbackInfo)
{
	switch (CallbackType)
	{
	case AK_EndOfEvent:
		return UAkEventCallbackInfo::Create((AkEventCallbackInfo*)CallbackInfo);
	case AK_Marker: 
		return UAkMarkerCallbackInfo::Create((AkMarkerCallbackInfo*)CallbackInfo);
	case AK_Duration: 
		return UAkDurationCallbackInfo::Create((AkDurationCallbackInfo*)CallbackInfo);
	case AK_Starvation: 
		return UAkEventCallbackInfo::Create((AkEventCallbackInfo*)CallbackInfo);
	case AK_MusicPlayStarted: 
		return UAkEventCallbackInfo::Create((AkEventCallbackInfo*)CallbackInfo);
	case AK_MusicSyncBeat:
	case AK_MusicSyncBar:
	case AK_MusicSyncEntry:
	case AK_MusicSyncExit:
	case AK_MusicSyncGrid:
	case AK_MusicSyncUserCue:
	case AK_MusicSyncPoint:
		return UAkMusicSyncCallbackInfo::Create((AkMusicSyncCallbackInfo*)CallbackInfo);
	case AK_MIDIEvent: 
		return UAkMIDIEventCallbackInfo::Create((AkMIDIEventCallbackInfo*)CallbackInfo);
	default: 
		return nullptr;
	}
	return nullptr;
}

AkCallbackType AkCallbackTypeHelpers::GetCallbackMaskFromBlueprintMask(int32 BlueprintCallbackType)
{
	return (AkCallbackType)BlueprintCallbackType;
}

EAkCallbackType AkCallbackTypeHelpers::GetBlueprintCallbackTypeFromAkCallbackType(AkCallbackType CallbackType)
{
	uint32 BitIndex = 0;
	uint32 CbType = (uint32)CallbackType >> 1;
	while (CbType != 0)
	{
		CbType >>= 1;
		BitIndex++;
	}
	return (EAkCallbackType)BitIndex;
}


UAkCallbackInfo::UAkCallbackInfo( class FObjectInitializer const & ObjectInitializer) :
	Super(ObjectInitializer)
{}

UAkCallbackInfo* UAkCallbackInfo::Create(AkGameObjectID GameObjectID)
{
	auto CbInfo = NewObject<UAkCallbackInfo>();
	if (CbInfo)
	{
		CbInfo->AkComponent = UAkComponent::GetAkComponent(GameObjectID);
	}
	return CbInfo;
}

UAkEventCallbackInfo::UAkEventCallbackInfo(class FObjectInitializer const & ObjectInitializer) :
	Super(ObjectInitializer)
{}

UAkEventCallbackInfo* UAkEventCallbackInfo::Create(AkEventCallbackInfo* AkEventCbInfo)
{
	auto CbInfo = NewObject<UAkEventCallbackInfo>();
	if (CbInfo)
	{
		CbInfo->AkComponent = UAkComponent::GetAkComponent(AkEventCbInfo->gameObjID);
		CbInfo->PlayingID = AkEventCbInfo->playingID;
		CbInfo->EventID = AkEventCbInfo->eventID;
	}
	return CbInfo;
}

UAkMIDIEventCallbackInfo::UAkMIDIEventCallbackInfo(class FObjectInitializer const & ObjectInitializer) :
	Super(ObjectInitializer)
{}

UAkMIDIEventCallbackInfo* UAkMIDIEventCallbackInfo::Create(AkMIDIEventCallbackInfo* AkMIDIEventCbInfo)
{
	auto CbInfo = NewObject<UAkMIDIEventCallbackInfo>();
	if (CbInfo)
	{
		CbInfo->AkComponent = UAkComponent::GetAkComponent(AkMIDIEventCbInfo->gameObjID);
		CbInfo->PlayingID = AkMIDIEventCbInfo->playingID;
		CbInfo->EventID = AkMIDIEventCbInfo->eventID;
		CbInfo->AkMidiEvent = AkMIDIEventCbInfo->midiEvent;
	}
	return CbInfo;
}

UAkMarkerCallbackInfo::UAkMarkerCallbackInfo(class FObjectInitializer const & ObjectInitializer) :
	Super(ObjectInitializer)
{}

UAkMarkerCallbackInfo* UAkMarkerCallbackInfo::Create(AkMarkerCallbackInfo* AkMarkerCbInfo)
{
	auto CbInfo = NewObject<UAkMarkerCallbackInfo>();
	if (CbInfo)
	{
		CbInfo->AkComponent = UAkComponent::GetAkComponent(AkMarkerCbInfo->gameObjID);
		CbInfo->PlayingID = AkMarkerCbInfo->playingID;
		CbInfo->EventID = AkMarkerCbInfo->eventID;
		CbInfo->Identifier = AkMarkerCbInfo->uIdentifier;
		CbInfo->Position = AkMarkerCbInfo->uPosition;
		CbInfo->Label = FString(AkMarkerCbInfo->strLabel);
	}
	return CbInfo;
}

UAkDurationCallbackInfo::UAkDurationCallbackInfo(class FObjectInitializer const & ObjectInitializer) :
	Super(ObjectInitializer)
{}

UAkDurationCallbackInfo* UAkDurationCallbackInfo::Create(AkDurationCallbackInfo* AkDurationCbInfo)
{
	auto CbInfo = NewObject<UAkDurationCallbackInfo>();
	if (CbInfo)
	{
		CbInfo->AkComponent = UAkComponent::GetAkComponent(AkDurationCbInfo->gameObjID);
		CbInfo->PlayingID = AkDurationCbInfo->playingID;
		CbInfo->EventID = AkDurationCbInfo->eventID;
		CbInfo->Duration = AkDurationCbInfo->fDuration;
		CbInfo->EstimatedDuration = AkDurationCbInfo->fEstimatedDuration;
		CbInfo->AudioNodeID = AkDurationCbInfo->audioNodeID;
		CbInfo->MediaID = AkDurationCbInfo->mediaID;
		CbInfo->bStreaming = AkDurationCbInfo->bStreaming;
	}
	return CbInfo;
}

UAkMusicSyncCallbackInfo::UAkMusicSyncCallbackInfo(class FObjectInitializer const & ObjectInitializer) :
	Super(ObjectInitializer)
{}

UAkMusicSyncCallbackInfo* UAkMusicSyncCallbackInfo::Create(AkMusicSyncCallbackInfo* AkMusicCbInfo)
{
	auto CbInfo = NewObject<UAkMusicSyncCallbackInfo>();
	if (CbInfo)
	{
		CbInfo->AkComponent = UAkComponent::GetAkComponent(AkMusicCbInfo->gameObjID);
		CbInfo->PlayingID = AkMusicCbInfo->playingID;
		CbInfo->SegmentInfo = AkMusicCbInfo->segmentInfo;
		CbInfo->MusicSyncType = AkCallbackTypeHelpers::GetBlueprintCallbackTypeFromAkCallbackType(AkMusicCbInfo->musicSyncType);
		CbInfo->UserCueName = FString(AkMusicCbInfo->pszUserCueName);
	}
	return CbInfo;
}

EAkMidiEventType UAkMIDIEventCallbackInfo::GetType()
{
	return (EAkMidiEventType)AkMidiEvent.byType;
}

uint8 UAkMIDIEventCallbackInfo::GetChannel()
{
	// Add one here so we report "Artist" channel number (between 1 and 16), instead of reporting the underlying value of 0-F.
	return AkMidiEvent.byChan + 1;
}

bool UAkMIDIEventCallbackInfo::GetGeneric(FAkMidiGeneric& AsGeneric)
{
	AsGeneric = FAkMidiGeneric(AkMidiEvent);
	return true;
}

bool UAkMIDIEventCallbackInfo::GetNoteOn(FAkMidiNoteOnOff& AsNoteOn)
{
	if (GetType() != EAkMidiEventType::AkMidiEventTypeNoteOn)
	{
		return false;
	}

	AsNoteOn = FAkMidiNoteOnOff(AkMidiEvent);
	return true;
}

bool UAkMIDIEventCallbackInfo::GetNoteOff(FAkMidiNoteOnOff& AsNoteOff)
{
	if (GetType() != EAkMidiEventType::AkMidiEventTypeNoteOff)
	{
		return false;
	}

	AsNoteOff = FAkMidiNoteOnOff(AkMidiEvent);
	return true;
}

bool UAkMIDIEventCallbackInfo::GetCc(FAkMidiCc& AsCc)
{
	if (GetType() != EAkMidiEventType::AkMidiEventTypeController)
	{
		return false;
	}

	AsCc = FAkMidiCc(AkMidiEvent);
	return true;
}

bool UAkMIDIEventCallbackInfo::GetPitchBend(FAkMidiPitchBend& AsPitchBend)
{
	if (GetType() != EAkMidiEventType::AkMidiEventTypePitchBend)
	{
		return false;
	}

	AsPitchBend = FAkMidiPitchBend(AkMidiEvent);
	return true;
}

bool UAkMIDIEventCallbackInfo::GetNoteAftertouch(FAkMidiNoteAftertouch& AsNoteAftertouch)
{
	if (GetType() != EAkMidiEventType::AkMidiEventTypeNoteAftertouch)
	{
		return false;
	}

	AsNoteAftertouch = FAkMidiNoteAftertouch(AkMidiEvent);
	return true;
}

bool UAkMIDIEventCallbackInfo::GetChannelAftertouch(FAkMidiChannelAftertouch& AsChannelAftertouch)
{
	if (GetType() != EAkMidiEventType::AkMidiEventTypeChannelAftertouch)
	{
		return false;
	}

	AsChannelAftertouch = FAkMidiChannelAftertouch(AkMidiEvent);
	return true;
}

bool UAkMIDIEventCallbackInfo::GetProgramChange(FAkMidiProgramChange& AsProgramChange)
{
	if (GetType() != EAkMidiEventType::AkMidiEventTypeProgramChange)
	{
		return false;
	}

	AsProgramChange = FAkMidiProgramChange(AkMidiEvent);
	return true;
}
