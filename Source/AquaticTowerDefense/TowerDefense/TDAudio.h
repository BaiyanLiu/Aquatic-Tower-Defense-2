// Aquatic Tower Defense - isometric underwater tower defense with mazing.

#pragma once

#include "CoreMinimal.h"
#include "TDTypes.h"

class UAudioComponent;

/** Sound effects (synthesized by Tools/make_sounds.py, imported into /Game/AquaticTowerDefense/Audio) */
enum class ETDSound : uint8
{
	Hit,
	Kill,
	ShieldPop,
	Leak,
	LevelUp,
	WaveStart,
	Build,
	Sell,
	ItemDrop,
	Click,
	Upgrade,
	TierUnlock,		// an Abyss tier unlocked (appended: the sounds above keep their numbers)
	BossSting,		// a boss wave is released: deep horn and drum hit
	Split,			// a sea star splits off a copy
	Count
};

/**
 *  Plays the game's sounds as 2D one-shots (the camera sees the whole board), with a short per-sound
 *  cooldown so dozens of octopuses don't stack into noise, plus the looping ambient music track.
 */
namespace TDAudio
{
	void Play(const UObject* WorldContext, ETDSound Sound, float Volume = 1.0f);

	/** The firing sound of a tower type (walls and aura towers are silent) */
	void PlayShot(const UObject* WorldContext, ETDTowerType Tower);

	/** A creature's own death sound (S_Death_<kind>), or the generic kill sound if it has none */
	void PlayDeath(const UObject* WorldContext, ETDEnemyType Enemy, float Volume = 1.0f);

	/** Music sets: each pairs an ambient loop with a boss loop on the same chords and 48 s length. The set follows the
	 *  map in play (checked in TickMusic), so each map has its own music; the switch fades across a second. */
	constexpr int32 MusicSetCount = 3;
	int32 MusicSetForMap(int32 Map);

	/** Effects and music volume, 0-1 (0 silences) */
	void SetVolumes(float Effects, float Music);

	/** Starts the music loops if they aren't playing (or stops them at zero volume) and applies the volume.
	 *  The ambient loop and the heavier boss loop (S_MusicBoss, same chords and 48 s length) always play together
	 *  in step; only their volumes change. */
	void UpdateMusic(const UObject* WorldContext);

	/** Crossfades to the boss loop (true) or back to the ambient loop (false) over BossFadeSeconds of real time */
	void SetBossMusic(bool bBoss);

	/** Pitch of both loops (Abyss tiers play the music a little lower) */
	void SetMusicPitch(float Pitch);

	/** Advances the crossfade and the music set change; pass real seconds so game speed doesn't change the fades */
	void TickMusic(float RealDeltaSeconds);

	constexpr float BossFadeSeconds = 2.0f;

	/** Asset names of every sound the game plays (effects, tower shots, creature deaths, every music set's loops), for the asset test */
	TArray<FString> AllSoundNames();
}
