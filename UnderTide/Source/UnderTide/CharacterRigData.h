#pragma once
// Body, fingers and cloth roots match the current imported skeletons.
static const TCHAR* BreachRigNames[4][17] = {
{TEXT("腰"),TEXT("上半身"),TEXT("上半身2"),TEXT("首"),TEXT("頭"),TEXT("左腕"),TEXT("左ひじ"),TEXT("左手首"),TEXT("右腕"),TEXT("右ひじ"),TEXT("右手首"),TEXT("左足D"),TEXT("左ひざD"),TEXT("左足首D"),TEXT("右足D"),TEXT("右ひざD"),TEXT("右足首D")},
{TEXT("腰"),TEXT("上半身"),TEXT("上半身2"),TEXT("首"),TEXT("頭"),TEXT("左腕"),TEXT("左ひじ"),TEXT("左手首"),TEXT("右腕"),TEXT("右ひじ"),TEXT("右手首"),TEXT("左足D"),TEXT("左ひざD"),TEXT("左足首D"),TEXT("右足D"),TEXT("右ひざD"),TEXT("右足首D")},
{TEXT("腰"),TEXT("上半身"),TEXT("上半身2"),TEXT("首"),TEXT("頭"),TEXT("左腕"),TEXT("左ひじ"),TEXT("左手首"),TEXT("右腕"),TEXT("右ひじ"),TEXT("右手首"),TEXT("左足D"),TEXT("左ひざD"),TEXT("左足首D"),TEXT("右足D"),TEXT("右ひざD"),TEXT("右足首D")},
{TEXT("mixamorig:Hips"),TEXT("mixamorig:Spine"),TEXT("mixamorig:Spine2"),TEXT("mixamorig:Neck"),TEXT("mixamorig:Head"),TEXT("arm_upper_L"),TEXT("arm_lower_L"),TEXT("hand_L"),TEXT("arm_upper_R"),TEXT("arm_lower_R"),TEXT("hand_R"),TEXT("LeftUpLeg_L"),TEXT("LeftLeg_L"),TEXT("LeftFoot_L"),TEXT("LeftUpLeg_R"),TEXT("LeftLeg_R"),TEXT("LeftFoot_R")}
};
static const TCHAR* BreachFingerNames[4][2][15] = {
{{TEXT("左親指０"),TEXT("左親指１"),TEXT("左親指２"),TEXT("左人指１"),TEXT("左人指２"),TEXT("左人指３"),TEXT("左中指１"),TEXT("左中指２"),TEXT("左中指３"),TEXT("左薬指１"),TEXT("左薬指２"),TEXT("左薬指３"),TEXT("左小指１"),TEXT("左小指２"),TEXT("左小指３")},{TEXT("右親指０"),TEXT("右親指１"),TEXT("右親指２"),TEXT("右人指１"),TEXT("右人指２"),TEXT("右人指３"),TEXT("右中指１"),TEXT("右中指２"),TEXT("右中指３"),TEXT("右薬指１"),TEXT("右薬指２"),TEXT("右薬指３"),TEXT("右小指１"),TEXT("右小指２"),TEXT("右小指３")}},
{{TEXT("左親指０"),TEXT("左親指１"),TEXT("左親指２"),TEXT("左人指１"),TEXT("左人指２"),TEXT("左人指３"),TEXT("左中指１"),TEXT("左中指２"),TEXT("左中指３"),TEXT("左薬指１"),TEXT("左薬指２"),TEXT("左薬指３"),TEXT("左小指１"),TEXT("左小指２"),TEXT("左小指３")},{TEXT("右親指０"),TEXT("右親指１"),TEXT("右親指２"),TEXT("右人指１"),TEXT("右人指２"),TEXT("右人指３"),TEXT("右中指１"),TEXT("右中指２"),TEXT("右中指３"),TEXT("右薬指１"),TEXT("右薬指２"),TEXT("右薬指３"),TEXT("右小指１"),TEXT("右小指２"),TEXT("右小指３")}},
{{TEXT("左親指０"),TEXT("左親指１"),TEXT("左親指２"),TEXT("左人指１"),TEXT("左人指２"),TEXT("左人指３"),TEXT("左中指１"),TEXT("左中指２"),TEXT("左中指３"),TEXT("左薬指１"),TEXT("左薬指２"),TEXT("左薬指３"),TEXT("左小指１"),TEXT("左小指２"),TEXT("左小指３")},{TEXT("右親指０"),TEXT("右親指１"),TEXT("右親指２"),TEXT("右人指１"),TEXT("右人指２"),TEXT("右人指３"),TEXT("右中指１"),TEXT("右中指２"),TEXT("右中指３"),TEXT("右薬指１"),TEXT("右薬指２"),TEXT("右薬指３"),TEXT("右小指１"),TEXT("右小指２"),TEXT("右小指３")}},
{{TEXT("finger_thumb_0_L"),TEXT("finger_thumb_1_L"),TEXT("finger_thumb_2_L"),TEXT("finger_index_0_L"),TEXT("finger_index_1_L"),TEXT("finger_index_2_L"),TEXT("finger_middle_0_L"),TEXT("finger_middle_1_L"),TEXT("finger_middle_2_L"),TEXT("finger_ring_0_L"),TEXT("finger_ring_1_L"),TEXT("finger_ring_2_L"),TEXT("finger_pinky_0_L"),TEXT("finger_pinky_1_L"),TEXT("finger_pinky_2_L")},{TEXT("finger_thumb_0_R"),TEXT("finger_thumb_1_R"),TEXT("finger_thumb_2_R"),TEXT("finger_index_0_R"),TEXT("finger_index_1_R"),TEXT("finger_index_2_R"),TEXT("finger_middle_0_R"),TEXT("finger_middle_1_R"),TEXT("finger_middle_2_R"),TEXT("finger_ring_0_R"),TEXT("finger_ring_1_R"),TEXT("finger_ring_2_R"),TEXT("finger_pinky_0_R"),TEXT("finger_pinky_1_R"),TEXT("finger_pinky_2_R")}}
};

// Authored cloth roots in the imported skeletons. Descendants form the simulated chains.
static const TCHAR* BreachClothRoots[4][20] = {
    {TEXT("披风1_0_0"),TEXT("披风1_0_1"),TEXT("披风1_0_2"),TEXT("披风2_0_0"),TEXT("披风2_0_1"),TEXT("披风2_0_2"),TEXT("领带_0_1"),nullptr},
    {TEXT("右袖一1"),TEXT("右袖二1"),TEXT("右袖三1"),TEXT("上袖1"),TEXT("上袖2"),TEXT("上袖3"),TEXT("左袖一1"),TEXT("左袖口"),TEXT("下摆_0_3"),TEXT("下摆_0_4"),TEXT("下摆_0_5"),TEXT("下摆_0_6"),TEXT("下摆_0_7"),TEXT("下摆_0_8"),TEXT("下摆_0_9"),TEXT("下摆_0_10"),TEXT("下摆_0_11"),TEXT("下摆_0_12"),TEXT("下摆_0_14"),nullptr},
    {TEXT("前裙_0_0"),TEXT("前裙_0_1"),TEXT("前裙_0_2"),TEXT("前裙_0_3"),TEXT("侧裙_0_0"),TEXT("侧裙_0_1"),TEXT("侧裙_0_2"),TEXT("侧裙_0_3"),TEXT("侧裙_0_4"),TEXT("侧裙_0_5"),TEXT("后裙_0_0"),TEXT("后裙_0_1"),TEXT("后裙_0_2"),TEXT("后裙_0_3"),TEXT("后裙_0_4"),TEXT("后裙_0_5"),TEXT("后裙_0_6"),TEXT("后裙_0_7"),nullptr},
    {TEXT("mixamorig:Hips.001"),TEXT("mixamorig:Hips.008"),TEXT("mixamorig:Hips.015"),TEXT("mixamorig:Hips.021"),TEXT("mixamorig:Hips.027"),TEXT("mixamorig:Hips.006"),TEXT("mixamorig:Hips.026"),TEXT("mixamorig:Hips.036"),TEXT("mixamorig:Hips.041"),TEXT("mixamorig:Hips.046"),nullptr},
};
