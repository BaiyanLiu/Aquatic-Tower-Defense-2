// Aquatic Tower Defense - isometric underwater tower defense with mazing.

#pragma once

#include "CoreMinimal.h"

class ATDPlayerController;

/**
 *  Development tools driven from the player controller's PlayerTick (they keep running while paused):
 *  - FTDUITest: a scripted test that drives every HUD path through simulated cursor moves and clicks
 *    (the same controller / HUD handlers as real input) and logs PASS / FAIL per step.
 *  - FTDSim: plays the game with a scripted builder and logs per-wave balance numbers and performance samples.
 *  Not compiled into shipping builds.
 */
class FTDDevTask
{
public:
	virtual ~FTDDevTask() = default;

	/** Returns false when finished */
	virtual bool Tick(float RealDeltaTime) = 0;
};

#if !UE_BUILD_SHIPPING

class FTDUITest : public FTDDevTask
{
public:
	explicit FTDUITest(ATDPlayerController* InPC);
	virtual bool Tick(float RealDeltaTime) override;

private:
	enum class EKind : uint8 { Do, Check, WaitFor, Wait };

	struct FStep
	{
		EKind Kind = EKind::Do;
		FString Name;
		TFunction<bool()> Fn;
		float Timeout = 0.0f;
	};

	void Do(const FString& Name, TFunction<void()> Action);
	void Check(const FString& Name, TFunction<bool()> Condition);
	void WaitFor(const FString& Name, TFunction<bool()> Condition, float Timeout);
	void Wait(float Seconds);

	void BuildScript();
	void Report(const FString& Name, bool bPassed, const FString& Extra = FString());

	TWeakObjectPtr<ATDPlayerController> PC;
	TArray<FStep> Steps;
	int32 Index = 0;
	float StepTime = 0.0f;
	int32 FramesToSkip = 0;
	int32 Passed = 0;
	int32 Failed = 0;
	TArray<FString> Failures;
};

class FTDSim : public FTDDevTask
{
public:
	FTDSim(ATDPlayerController* InPC, int32 InStrategy, int32 InWaves, float InTimeScale);
	virtual bool Tick(float RealDeltaTime) override;

private:
	void Start();
	void Build();
	/** Strategy 1: greedy builder that places towers and walls where they cover the most of the current route */
	void BuildAdaptive();
	void ManageTowers();
	void LogWave(int32 WaveNumber);
	void SamplePerformance(float RealDeltaTime);
	void Finish(const TCHAR* Outcome);

	TWeakObjectPtr<ATDPlayerController> PC;
	int32 Strategy = 1;
	int32 Waves = 30;
	float TimeScale = 4.0f;
	bool bStarted = false;
	int32 PlanIndex = 0;
	/** Strategy 1: position in the tower rotation (one full pass counts as the plan being done) */
	int32 RotationIndex = 0;
	/** Strategy 1: route length of the empty board, and pearls spent so far by kind */
	int32 NaturalPath = 0;
	int32 TowerSpent = 0;
	int32 WallSpent = 0;
	int32 SkillSpent = 0;
	int32 ItemSpent = 0;
	bool bMazeLogged = false;
	/** Strategy 1: board as text (# wall, R rock, letters for towers, . route), one log line per row */
	void LogBoard(const TCHAR* Label);
	/** Strategy 1: raises the level of items on the towers that deal the most damage */
	void UpgradeItems();
	void LogTopTowers();
	/** Takes the boon a typical player would from the open offer (never a cursed one) */
	void PickBoon();
	/** Closest any creature of the wave in play came to the reef, as a share of its way (0 = at the spawn, 1 = leaked) */
	void SampleDanger();
	float WaveDanger = 0.0f;
	int32 WaveLivesStart = -1;
	int32 LastLoggedWave = 0;
	float BuildTimer = 0.0f;
	double StartTime = 0.0;

	// performance accumulators
	float SampleTimer = 0.0f;
	int32 SampleFrames = 0;
	double FrameMsSum = 0.0, GameMsSum = 0.0, RenderMsSum = 0.0, GpuMsSum = 0.0;
	double DrawCallsSum = 0.0, PrimitivesSum = 0.0;
	double TotalFrameMs = 0.0, TotalGameMs = 0.0, TotalRenderMs = 0.0, TotalGpuMs = 0.0, TotalDrawCalls = 0.0;
	int32 TotalSamples = 0;
	float WorstFrameMs = 0.0f;
	int32 PeakEnemies = 0;
};

/** Screenshots every panel and menu in turn, then quits. Works with -RenderOffscreen, so no window opens. */
class FTDTour : public FTDDevTask
{
public:
	explicit FTDTour(ATDPlayerController* InPC) : PC(InPC) {}
	virtual bool Tick(float RealDeltaTime) override;

private:
	TWeakObjectPtr<ATDPlayerController> PC;
	int32 Index = 0;
	float Timer = 2.0f;
};

/** Repeatable benchmark: the sim's maze with towers firing at Count creatures, frame rate uncapped */
class FTDPerf : public FTDDevTask
{
public:
	FTDPerf(ATDPlayerController* InPC, int32 InCount) : PC(InPC), Count(InCount) {}
	virtual bool Tick(float RealDeltaTime) override;

private:
	void Sample(float RealDeltaTime);
	void Report(const TCHAR* Label);

	TWeakObjectPtr<ATDPlayerController> PC;
	int32 Count = 100;
	int32 Phase = 0;
	float PhaseTime = 0.0f;
	int32 Spawned = 0;
	bool bShotTaken = false;
	bool bCsvRunning = false;
	TArray<float> FrameTimes;
	double Frames = 0.0, FrameMs = 0.0, GameMs = 0.0, RenderMs = 0.0, GpuMs = 0.0, DrawCalls = 0.0, Primitives = 0.0, WorstMs = 0.0;
};

/** Model gallery: pairs of models (original and detailed shapes) laid out away from the board, one screenshot per page */
class FTDGallery : public FTDDevTask
{
public:
	FTDGallery(ATDPlayerController* InPC, int32 InPairsPerPage, int32 InFirst, int32 InLast);
	virtual bool Tick(float RealDeltaTime) override;

private:
	void BuildPage();
	void ClearPage();

	TWeakObjectPtr<ATDPlayerController> PC;
	int32 PairsPerPage = 8;
	int32 First = 0;
	int32 Last = 0;
	int32 Next = 0;
	int32 Page = 0;
	int32 Phase = 0;
	float Timer = 0.0f;
	TArray<TWeakObjectPtr<AActor>> Spawned;
};

#endif
