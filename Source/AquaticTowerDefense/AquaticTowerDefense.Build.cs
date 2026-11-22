// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using UnrealBuildTool;

public class AquaticTowerDefense : ModuleRules
{
	public AquaticTowerDefense(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "SlateCore", "RHI", "RenderCore", "MeshDescription", "StaticMeshDescription" });

		PublicIncludePaths.AddRange(new string[] {
			"AquaticTowerDefense",
			"AquaticTowerDefense/TowerDefense"
		});

		WriteIconSourceHash();
	}

	/** The painted HUD icons are cached on disk (Saved/IconCache) under a hash of the painter's source, written here as
	 *  TDIconsSourceHash.h: only an edit to the icon code repaints them, not every rebuild of TDIcons.cpp */
	private void WriteIconSourceHash()
	{
		try
		{
			string IconDir = Path.Combine(ModuleDirectory, "TowerDefense");
			string[] Files = Directory.GetFiles(IconDir, "TDIcons*.*")
				.Where(F => F.EndsWith(".cpp") || F.EndsWith(".h") || F.EndsWith(".inl"))
				.OrderBy(F => Path.GetFileName(F), StringComparer.OrdinalIgnoreCase)
				.ToArray();
			StringBuilder All = new StringBuilder();
			foreach (string File in Files)
			{
				All.Append(Path.GetFileName(File)).Append('\n');
				All.Append(System.IO.File.ReadAllText(File).Replace("\r\n", "\n"));
			}
			string Hash;
			using (SHA1 Sha = SHA1.Create())
			{
				Hash = BitConverter.ToString(Sha.ComputeHash(Encoding.UTF8.GetBytes(All.ToString()))).Replace("-", "").Substring(0, 20);
			}

			string GeneratedDir = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "..", "Intermediate", "ATDGenerated"));
			Directory.CreateDirectory(GeneratedDir);
			string Header = Path.Combine(GeneratedDir, "TDIconsSourceHash.h");
			string Contents = "// Written by AquaticTowerDefense.Build.cs from the icon painter's source; do not edit\n#pragma once\n#define ATD_ICON_SOURCE_HASH \"" + Hash + "\"\n";
			if (!System.IO.File.Exists(Header) || System.IO.File.ReadAllText(Header) != Contents)
			{
				System.IO.File.WriteAllText(Header, Contents);
			}
			PrivateIncludePaths.Add(GeneratedDir);
		}
		catch (Exception Ex)
		{
			// TDIcons.cpp falls back to its build date and time
			Console.WriteLine("AquaticTowerDefense: could not hash the icon source ({0})", Ex.Message);
		}
	}
}
