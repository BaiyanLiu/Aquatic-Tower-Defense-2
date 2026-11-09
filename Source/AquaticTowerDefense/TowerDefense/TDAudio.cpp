// Aquatic Tower Defense - isometric underwater tower defense with mazing.

#include "TDAudio.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundBase.h"
#include "Engine/World.h"

namespace
{
	const TCHAR* SoundName(ETDSound Sound)
	{
		switch (Sound)
		{
		case ETDSound::Hit:			return TEXT("S_Hit");
		case ETDSound::Kill:		return TEXT("S_Kill");
		case ETDSound::ShieldPop:	return TEXT("S_ShieldPop");
		case ETDSound::Leak:		return TEXT("S_Leak");
		case ETDSound::LevelUp:		return TEXT("S_LevelUp");
		case ETDSound::WaveStart:	return TEXT("S_WaveStart");
		case ETDSound::Build:		return TEXT("S_Build");
		case ETDSound::Sell:		return TEXT("S_Sell");
		case ETDSound::ItemDrop:	return TEXT("S_ItemDrop");
		case ETDSound::Click:		return TEXT("S_Click");
		case ETDSound::Upgrade:		return TEXT("S_Upgrade");
		case ETDSound::TierUnlock:	return TEXT("S_TierUnlock");
		default:					return nullptr;
		}
	}

	const TCHAR* ShotName(ETDTowerType Tower)
	{
		switch (Tower)
		{
		case ETDTowerType::Harpoon:	return TEXT("S_Shot_Harpoon");
		case ETDTowerType::Jelly:	return TEXT("S_Shot_Jelly");
		case ETDTowerType::Puffer:	return TEXT("S_Shot_Puffer");
		case ETDTowerType::Eel:		return TEXT("S_Shot_Eel");
		case ETDTowerType::Mantis:	return TEXT("S_Shot_Mantis");
		case ETDTowerType::Octopus:	return TEXT("S_Shot_Rock");
		case ETDTowerType::Lionfish:	return TEXT("S_Shot_Toxin");
		case ETDTowerType::Sawfish:	return TEXT("S_Shot_Razor");
		default:					return nullptr;
		}
	}

	/** Loaded sounds by asset name; rooted so they survive level changes */
	USoundWave* Load(const TCHAR* Name)
	{
		static TMap<FName, USoundWave*> Cache;
		if (!Name)
		{
			return nullptr;
		}
		if (USoundWave** Found = Cache.Find(FName(Name)))
		{
			return *Found;
		}
		USoundWave* Wave = LoadObject<USoundWave>(nullptr, *FString::Printf(TEXT("/Game/AquaticTowerDefense/Audio/%s.%s"), Name, Name));
		if (Wave)
		{
			Wave->AddToRoot();
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Aquatic Tower Defense: missing sound %s"), Name);
		}
		Cache.Add(FName(Name), Wave);
		return Wave;
	}

	/** Last real time each sound played, for the anti-spam cooldown */
	TMap<FName, double>& LastPlayed()
	{
		static TMap<FName, double> Times;
		return Times;
	}

	float EffectsScale = 1.0f;
	float MusicScale = 1.0f;
	TWeakObjectPtr<UAudioComponent> Music;
	TWeakObjectPtr<UAudioComponent> BossMusic;

	/** Boss loop share (0 = ambient only, 1 = boss only) and where it's heading */
	float BossMix = 0.0f;
	float BossTarget = 0.0f;
	float MusicPitch = 1.0f;

	/** The boss loop is a little louder by nature (drums), so it plays a touch quieter */
	constexpr float BossLoopLevel = 0.85f;

	constexpr float SfxVolume = 0.55f;
	constexpr float MusicVolume = 0.32f;

	void PlayNamed(const UObject* WorldContext, const TCHAR* Name, float Volume, double Cooldown)
	{
		if (EffectsScale <= 0.0f || !Name || !WorldContext || !WorldContext->GetWorld())
		{
			return;
		}
		const double Now = WorldContext->GetWorld()->GetRealTimeSeconds();
		double& Last = LastPlayed().FindOrAdd(FName(Name), -100.0);
		if (Now - Last < Cooldown && Now >= Last)
		{
			return;
		}
		Last = Now;
		if (USoundWave* Wave = Load(Name))
		{
			// a little pitch variety keeps repeated shots from sounding mechanical
			UGameplayStatics::PlaySound2D(WorldContext, Wave, Volume * SfxVolume * EffectsScale, FMath::FRandRange(0.94f, 1.06f));
		}
	}
}

void TDAudio::Play(const UObject* WorldContext, ETDSound Sound, float Volume)
{
	const double Cooldown = (Sound == ETDSound::Hit) ? 0.07 : (Sound == ETDSound::Kill ? 0.05 : 0.03);
	PlayNamed(WorldContext, SoundName(Sound), Volume, Cooldown);
}

void TDAudio::PlayShot(const UObject* WorldContext, ETDTowerType Tower)
{
	// lionfish and sawfish fire constantly: keep them quiet and throttled harder
	const bool bQuiet = Tower == ETDTowerType::Lionfish || Tower == ETDTowerType::Sawfish;
	PlayNamed(WorldContext, ShotName(Tower), bQuiet ? 0.35f : ((Tower == ETDTowerType::Octopus || Tower == ETDTowerType::Mantis) ? 0.6f : 0.8f), bQuiet ? 0.35 : 0.06);
}

namespace
{
	/** Applies the music volume and the crossfade to both loops. Never exactly zero: a silent loop keeps its place,
	 *  so the two stay in step. */
	void ApplyMusicMix()
	{
		const float Level = MusicVolume * FMath::Max(MusicScale, 0.001f);
		// equal-power crossfade, so the music doesn't dip in the middle of the fade
		const float Ambient = FMath::Cos(BossMix * HALF_PI);
		const float Boss = FMath::Sin(BossMix * HALF_PI);
		if (UAudioComponent* Component = Music.Get())
		{
			Component->SetVolumeMultiplier(FMath::Max(Level * Ambient, 0.0001f));
		}
		if (UAudioComponent* Component = BossMusic.Get())
		{
			Component->SetVolumeMultiplier(FMath::Max(Level * Boss * BossLoopLevel, 0.0001f));
		}
	}

	USoundWave* LoadLoop(const TCHAR* Name)
	{
		USoundWave* Wave = Load(Name);
		if (Wave)
		{
			Wave->bLooping = true;
			// keep playing while silent, so the loop that's faded out stays in step with the other
			Wave->VirtualizationMode = EVirtualizationMode::PlayWhenSilent;
		}
		return Wave;
	}
}

void TDAudio::SetVolumes(float Effects, float InMusic)
{
	EffectsScale = FMath::Clamp(Effects, 0.0f, 1.0f);
	MusicScale = FMath::Clamp(InMusic, 0.0f, 1.0f);
	ApplyMusicMix();
}

void TDAudio::SetBossMusic(bool bBoss)
{
	BossTarget = bBoss ? 1.0f : 0.0f;
}

void TDAudio::SetMusicPitch(float Pitch)
{
	MusicPitch = FMath::Clamp(Pitch, 0.5f, 2.0f);
	for (UAudioComponent* Component : { Music.Get(), BossMusic.Get() })
	{
		if (Component)
		{
			Component->SetPitchMultiplier(MusicPitch);
		}
	}
}

void TDAudio::TickMusic(float RealDeltaSeconds)
{
	if (BossMix == BossTarget)
	{
		return;
	}
	const float Step = FMath::Max(0.0f, RealDeltaSeconds) / BossFadeSeconds;
	BossMix = BossTarget > BossMix ? FMath::Min(BossTarget, BossMix + Step) : FMath::Max(BossTarget, BossMix - Step);
	ApplyMusicMix();
}

void TDAudio::UpdateMusic(const UObject* WorldContext)
{
	UAudioComponent* Component = Music.Get();
	UAudioComponent* Boss = BossMusic.Get();
	if (MusicScale <= 0.0f)
	{
		for (UAudioComponent* Each : { Component, Boss })
		{
			if (Each)
			{
				Each->Stop();
			}
		}
		return;
	}
	if (Component && Component->IsPlaying())
	{
		ApplyMusicMix();
		return;
	}
	USoundWave* Wave = LoadLoop(TEXT("S_Music"));
	USoundWave* BossWave = LoadLoop(TEXT("S_MusicBoss"));
	if (!Wave || !WorldContext)
	{
		return;
	}
	// both survive level reloads (restart / load) so the loops don't restart every time
	if (!Component)
	{
		Component = UGameplayStatics::CreateSound2D(WorldContext, Wave, MusicVolume * MusicScale, MusicPitch, 0.0f, nullptr, true, false);
		Music = Component;
	}
	if (!Boss && BossWave)
	{
		Boss = UGameplayStatics::CreateSound2D(WorldContext, BossWave, 0.0001f, MusicPitch, 0.0f, nullptr, true, false);
		BossMusic = Boss;
	}
	// started together (and again together after the music was switched off), so the two loops stay in step
	for (UAudioComponent* Each : { Component, Boss })
	{
		if (Each)
		{
			Each->SetPitchMultiplier(MusicPitch);
			Each->Play();
		}
	}
	ApplyMusicMix();
}
