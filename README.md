# UnderTide

正在试图学习UE，明年能找到实习吗...

好焦虑好焦虑好焦虑好崩溃好崩溃好难好难好难怎么这么难

---

## 场景大纲

`Arena` 大纲按 `01_Structure`、`02_Props_and_Cover`、`03_Operator_Displays`、`04_Lighting`、`05_Post_Process` 和 `06_Gameplay` 分组；对象名称同样只使用英文与 ASCII 符号，并以坐标区分同类部件，便于脚本、版本管理和跨系统工具处理。`BreachArena.cpp` 的 `OrganizeArenaOutliner` 只补充编辑器中的默认名称和空文件夹，保留手工命名与分组；`BuildArena` / `BakeArena` 生成场景时自动沿用。整理已有地图可单独调用该入口，不必重新生成场景。

## 模型与动作清单

以下是项目当前实际使用的角色模型、动画资源和输入动作。四个角色均使用完整人物模型；第一人称视角只隐藏本地视角中的头部，世界模型仍保留完整身体并投射影子，方便后续多人联机。

### 角色模型

| 角色 | UE Skeletal Mesh | 原始模型文件 | 用途 |
| --- | --- | --- | --- |
| 优菈 | `/Game/Characters/Eula/SK_Eula` | `SourceAssets/Eula/优菈.pmx` | 玩家、敌人展示、选人页面 |
| 黄泉 | `/Game/Characters/Acheron/SK_Acheron` | `model/` 中用户提供的黄泉模型（轴修复版 PMX） | 玩家、敌人展示、选人页面 |
| 李织烟 | `/Game/Characters/Lizhiyan/SK_Lizhiyan` | `SourceAssets/Lizhiyan/李织烟.pmx` | 玩家、敌人展示、选人页面 |
| 阿斯卡纶 | `/Game/Characters/Ascalon/SK_Ascalon` | `model/` 中用户提供的阿斯卡纶 `askl.pmx` | 玩家、敌人展示、选人页面 |

角色使用各自的 Skeleton、材质和纹理。当前第一人称实现使用上表中的完整 `SK_*` 模型；旧角色保留的 `FPArms` 辅助资源不参与运行。黄泉、阿斯卡纶替换了原索引 1、3 的联动优菈和木偶，旧模型和动画移出 Content，原始压缩包保留。

角色、敌人和随身武器在常态下通过 Custom Stencil 1 接入 `/Game/Materials/PP_UnderTideToon_Normal` 风格化后处理。该 Shader 采用偏现代二次元的写实融合方向：保留原贴图、PBR 光照和粗糙度细节，仅在角色范围内轻量压缩明暗、给暗部加入冷色倾向、收束高光，并用低透明度屏幕空间轮廓帮助角色与环境分离；场景本身不做色阶化。主要参数在材质编辑器中集中调节，HLSL 源码位于 `UnderTide/Shaders/Private/UnderTideToonNormal.ush`，需要重建材质时运行 `UnderTide/Scripts/create_toon_shader.py`。`Normal` 后缀明确表示这是常态着色，后续特殊状态使用独立材质和 Shader 名称。

四张手工头像位于 `/Game/Characters/Portraits/T_<Key>_Portrait`，选人页保留用户修改的竖版卡片，战斗 HUD 使用同一套贴图。更换角色资源不会重新生成或覆盖这些头像。

阿斯卡纶当前网格已删除 `MI_Ascalon_5` 中独立绑定左前臂的多余袖箭，保留该材质下的衣摆及已有模型修订。编辑器辅助入口 `RemoveAscalonSleeveBlade` 按材质、连通区域和现有骨骼映射定位，只修改当前网格，不重新导入原 PMX；默认仅检查，应用前需将当前 `SK_Ascalon.uasset` 备份至 `Saved/`。

### 枪械模型与选人页武器选择

`model/gun/` 中用户提供的四个压缩包已分别导入为静态网格，各目录包含独立材质和贴图。选人页右侧提供 AK、M4、MP5、AA12 的模型预览卡片，点击后显示“已选择”，分别记住优菈、李织烟、阿斯卡纶在本局中的选择；切换角色或关闭后重开选人页不会丢失选择。黄泉不显示武器面板，也没有对应点击区域，仍固定持刀。重开整局会重置为默认 AK。

四把枪均已接入实战：三名普通角色默认装备 AK；选人页选择后，会使用对应的导入模型、枪口、双手握持和战斗参数。黄泉保持固定持刀。预览使用独立场景中的模型捕获，不覆盖手工角色头像，也不重播或改变角色入场动作。`/Game/Weapons` 已加入始终烘焙目录。

持枪姿态参考用户放在 `UnderTide/TEMP/` 的四枪正面、侧面共八张图片：AK 和 MP5 左手托住护木，M4 左手前伸握住前部支撑位置，AA12 左手握弹鼓前方的垂直握把；右手保持扳机握持，双肘下收。四枪各自的握点、手腕朝向和第一人称位置集中在 `BreachWeapons.h` 的 `GunHolds`。外部模型按当前角色肩膀定位枪托，双手跟随世界枪械；第一人称沿用完整身体和稳定相机，使用自己的枪械构图，换弹与后坐时手腕随枪械旋转。

开镜动作参考用户提供的 `UnderTide/TEMP/AK.mov`、`M4.mov`、`MP5.mov`、`AA12.mov` 中各两遍演示，适配到当前第一人称玩法：枪械从右下方沿弧线抬起，带轻微倾斜后摆正收稳；AK、M4、MP5、AA12 的进入时长分别为 0.30、0.30、0.24、0.26 秒，退出分别为 0.22、0.22、0.18、0.20 秒。抬枪和 110°→46° 视野变化使用同一个可逆进度，中途松开或再次按住右键沿当前姿势反向过渡；完全开镜后停止枪械步行浮动。各枪终点位置在 `GunHolds`，弧线、倾角、时长和瞄准俯仰在 `BreachWeapons.h` 的 `GunAims`；外部模型独立抬枪抵肩。AA12 的独立镜片材质 `M_AA12_01` 改为低透明度透视玻璃，避免黑面挡住目标，其余枪械材质和网格保留。视频仅作为视觉参考，没有提取原游戏动画或新增第三人称切换。

正侧面对照可使用 `UnrealEditor-Cmd.exe <工程> /Game/Maps/Arena -game -BreachWeaponTest -BreachGunPoseReview -BreachWeaponCapture -RenderOffscreen -unattended -ResX=1600 -ResY=900`。它依次检查优菈、李织烟、阿斯卡纶的四枪握点、开镜缩放同步和中途反向，输出 `Saved/gun_pose_review.txt` 和 `Saved/GunPose_<角色索引>_<枪型>_<视角>.png`；视角包括 `Front`、`Side`、`Hip`、`Lift`、`Aim`、`Reverse`、`AimRepeat`、`HipReturn`、`AimFront`、`AimSide`。去掉截图参数并添加 `-nullrhi` 可只检查逻辑和握点距离。截图检查必须保留渲染，不能使用 `-nullrhi`。

AK 的弹匣容量为 **25 发**，单发身体伤害 **38**（原型为 34），爆头 76；射击间隔仍为 0.105 秒，换弹仍为 1.55 秒。静止时腰射散射半角约 1.03°、瞄准约 0.29°，分别为原型的 4.5 和 5 倍；连射额外散射最高约 0.80°，瞄准仅叠加该扩散的 45%，停止射击后逐渐恢复。AK 的视角垂直后坐输入是原型的 3.8 倍（瞄准 2.8 倍），另有小幅随机水平后坐；枪身后坐可累积且恢复更慢。

M4 的弹匣容量为 **30 发**，单发身体伤害 **35**、爆头 70，射击间隔 **0.09 秒**（约 667 RPM），换弹仍为 1.55 秒。静止时腰射散射半角约 0.69°、瞄准约 0.20°；单发扩散、最大连射扩散、镜头垂直/水平后坐和枪身后坐都低于 AK，停止射击后的散射与枪身恢复更快。它以较低单发伤害换取更好的可控性和稍高射速。

MP5 的弹匣容量为 **40 发**，10 米内单发身体伤害 **30**、爆头 60，射击间隔 **0.06 秒**（1000 RPM）。腰射散射半角约 0.52°、瞄准约 0.14°，连射扩散、镜头水平/垂直后坐和枪身后坐均低于 M4。伤害在超过 10 米后降至 23，使 10–30 米的理论身体 DPS 约为 383，与 AK 的约 362 和 M4 的约 389 大致持平；30–40 米继续线性衰减至 12，40 米后的理论身体 DPS 约为 200，显著低于两把步枪。10 米内理论身体 DPS 为 500，高于两把步枪。

AA12 使用 **8 发**弹鼓，射击间隔 **0.22 秒**（约 273 RPM）。每发生成 **8 颗**独立弹丸，每颗分别计算散射、命中部位和距离伤害；10 米内单颗身体伤害 **14**，全部命中为 112 伤害、理论身体 DPS 约 509，只略高于 MP5 的 500。超过 10 米后单颗伤害快速线性衰减，15 米起最低为 1；腰射散射半角约 3.15°，瞄准约 2.01°，强调贴身全弹丸命中的收益。

AK、M4、MP5、AA12 与资源缺失时的原型回退分别记住弹匣剩余弹数，共享备用弹药；切换武器、角色、黄泉往返或收枪不会补弹。换弹中途切换枪械会取消未完成的换弹。按 `3` 收枪、`1` 重新持枪仍有效；隐藏的四把导入枪械和原型零件同时关闭普通及隐藏投影，避免重复枪影。当前换弹沿用程序化压枪、左手动作与计时，静态网格没有独立弹匣拆装动画；武器状态尚未完成联机同步。

本地专项验证可运行 `UnderTide/Scripts/VerifyWeapon.ps1 -Render`，或通过 `-BreachWeaponTest -BreachWeaponCapture` 启动游戏；报告为 `Saved/weapon_test.txt`，四枪各生成 `Hip`、`Aim`、`Recoil`、`Reload` 四张第一人称截图，M4、MP5 与 AA12 另生成外部视角截图。实现与参数分别位于 `BreachGun.cpp`、`BreachWeapons.h`，射击沿用 `BreachCharacter.cpp`。

| 枪型 | UE Static Mesh | 原始压缩包 | 当前材质 |
| --- | --- | --- | --- |
| AK（源模型名 AKM） | `/Game/Weapons/AK/SM_AK` | `T_Weap_AKM_201_by_优姬在睡觉_*.zip` | 原包两张主体贴图 |
| M4（M4A1，靛蓝逐罪） | `/Game/Weapons/M4/SM_M4` | `S201靛蓝逐罪_by_桃乐丝啊_*.zip` | 原皮配色、法线及 RMO 贴图 |
| MP5（圆舞曲） | `/Game/Weapons/MP5/SM_MP5` | `圆舞曲_by_慕Qes_*.zip` | Blender 内嵌贴图及材质通道 |
| AA12（鸣火） | `/Game/Weapons/AA12/SM_AA12` | `S000 AA12-鸣火_by_优姬在睡觉_*.zip` | 枪身、弹匣及附属件原包贴图 |

M4 原包的 OBJ 引用了未提供的 MTL，导入时按材质槽匹配原皮贴图并重建基础材质，未还原原游戏的晶体、变色等特殊着色效果；粉皮贴图和 PSK 附件保留在原始包中。AK、AA12 的 PMX 骨架及蒙皮也保留在原始包中，静态网格不包含可驱动的骨骼或动画。MP5、M4 保留源文件尺寸，AK、AA12 按每 PMX 单位 8 cm 转换；四把实战枪械共用武器根节点的 0.8 倍缩放，各自使用独立网格偏移和枪口位置。

来源与使用条件：四个包均由用户提供；AK、AA12 包内注明模型来自《卡拉彼丘》、版权属于 Day1 工作室、配布者为“优姬在睡觉”，并限制商业使用、二次配布以及原说明列出的其他用途。M4、MP5 的包名分别标注“桃乐丝啊”和“慕Qes”，包内未找到明确的再分发许可。上述模型均不属于 Quaternius 的 CC0 动作授权范围，原始包和使用说明保留。

本地转换、导入和独立双侧预览脚本分别为 `Tools/prepare_guns.py`、`UnderTide/Scripts/import_guns.py`、`UnderTide/Scripts/review_guns.py`，沿用本地工具忽略规则。中间文件位于 `SourceAssets/Converted/Guns/`，检查报告与截图位于 `UnderTide/Saved/GunImport/`；运行游戏不依赖这些工具或源文件。

### 第一人称开镜准星

按用户提供的四张截图绘制四枪独立准星：AA12 为橙色分段小圆环与红点，AK 为金色棱角冠形与红点，M4 为蓝色开口框线与红点，MP5 为青白色开口圆环、侧翼星形与红点。参考轮廓保留原有 20% 放大，并随实际相机视野同步缩放。按用户圈定的裁剪范围，将开镜视野从 76° 收紧至 46°，枪械、准星和场景整体约为上一版的 1.84 倍；常规视野仍为 110°。随屏幕高度等比缩放，不因宽屏拉伸；AA12 小环保持在实体镜框内。中心红点对齐相机射击中心，装饰外框不表示实际散射范围。

仅当前玩家的第一人称开镜显示，抬枪接近完成时淡入；松开右键立即隐藏。腰射、换弹、收枪、黄泉持刀、外部相机、选人、暂停和死亡时隐藏。实现集中在 `BreachReticleHUD.cpp`，不需要新增贴图或重新导入枪械。

专项检查：`UnrealEditor-Cmd.exe <工程> /Game/Maps/Arena -game -BreachWeaponTest -BreachReticleReview -BreachWeaponCapture -RenderOffscreen -ForceRes -windowed -ResX=1600 -ResY=900 -unattended`。输出 `Saved/reticle_review.txt` 与 `Saved/Reticle_<枪型>_<Hip/Aim/Release/Reload/Unarmed>.png`，覆盖四枪开镜、再次开镜、退出、换弹、外部视角、暂停/选人恢复、死亡和近战显隐。可修改分辨率复核缩放；无渲染运行去掉截图参数并加 `-nullrhi`。

### 动画来源

- 女性动作来源：Quaternius **Animated Women Pack**，官方页面：<https://quaternius.com/packs/animatedwomen.html>
- 授权：CC0 1.0，可用于个人和商业项目。
- 原始动作文件：`Female_Alternative.fbx`。
- 源骨架动作名：`HumanArmature|Female_*`。
- 处理方式：30 FPS 采样、坐标系转换、按四个角色各自的骨骼映射重定向为 UE `AnimSequence`；空中跳跃不使用源动作位移，跳跃高度由角色移动组件控制。

### 当前使用的动画资源

#### 常规移动（每个角色一套）

资源目录：`/Game/Animations/Locomotion/<角色>/`

| 动画资源 | 游戏中的作用 | 触发方式 |
| --- | --- | --- |
| `A_<角色>_Idle_Loop` | 站立待机循环 | 不移动时 |
| `A_<角色>_Jog_Fwd_Loop` | 持枪前进 | 优菈、李织烟、阿斯卡纶持枪移动 |
| `A_<角色>_Sprint_Loop` | 快速奔跑 | 普通角色按 `3` 收枪，或黄泉持刀移动 |
| `A_<角色>_Crouch_Idle_Loop` | 下蹲待机 | 按住 `Ctrl` 且不移动 |
| `A_<角色>_Crouch_Fwd_Loop` | 下蹲移动 | 按住 `Ctrl` 移动 |
| `A_<角色>_Female_Jump_Start` | 女性化起跳、离地、收腿 | 按 `Space` 起跳 |
| `A_<角色>_Female_Jump_Air` | 空中收膝到伸腿 | 跳跃上升和下降阶段 |
| `A_<角色>_Female_Jump_Land` | 伸腿落地、站稳 | 角色接触地面 |

跳跃动作来自同一条女性动作 `HumanArmature|Female_Jump`，在项目中拆成了 `Start / Air / Land` 三段。`Female_Jump_Air` 会根据实际垂直速度从收腿过渡到伸腿。第一人称在起跳、滞空和落地阶段把双手稳定在镜头下缘，并停止复制世界模型的动态布料偏转，避免手臂、衣袖和衣摆随跳跃动作扫入画面；主动挥拳或挥刀仍优先显示，世界视角继续播放完整跳跃和布料动作。

#### 敌人死亡

资源目录：`/Game/Animations/Death/`

| 动画资源 | 角色 |
| --- | --- |
| `A_Eula_Death01` | 优菈敌人 |
| `A_Acheron_Death01` | 黄泉敌人 |
| `A_Lizhiyan_Death01` | 李织烟敌人 |
| `A_Ascalon_Death01` | 阿斯卡纶敌人 |

敌人被击杀后播放对应的 `Death01` 倒地动作，之后保留倒地模型一段时间再清理；四个角色分别使用与自身骨骼匹配的资源。

#### 选人页面入场和定格

资源目录：`/Game/Animations/Entrance/<角色>/`。优菈和李织烟保留原有女性动作；新角色使用专属入场，并保留 `Female_Idle` 作为安全回退和预览采样。

| 源动作 | 选人页面中的作用 |
| --- | --- |
| `Female_Standing` | 起身站定 |
| `Female_Clapping` | 拍手亮相 |
| `Female_Punch` | 出拳亮相 |
| `Female_Jump` | 轻跳亮相 |
| `Female_Idle` | 动态头像捕获与姿态检查；当前 UI 使用用户提供的静态头像 |

当前角色与入场动作的对应关系：

| 角色 | 选中后播放的入场动作 | 结束后 |
| --- | --- | --- |
| 优菈 | 用户提供 `eula-v3.vmd` 的前 78 帧，最后 0.9 秒混合到待机 `pose.vmd` | 保持用户提供的侧身姿势 |
| 黄泉 | Quaternius `Sword_Attack` 的蓄势、挥刀、收势片段，配原模型的刀与刀鞘 | 保持持刀姿势，左手持鞘 |
| 李织烟 | 以 `Female_Idle` 为基础，播放专属抱猫、低头轻靠动作 | 人物与猫一起保持抱持姿势 |
| 阿斯卡纶 | 用户提供的 Mixamo `Catwalk Sequence 03`，截取 3.5–7 秒的走近、转身和展示 | 保持扶腰姿势，不播放离场段 |

四个入场展示统一为 **3.5 秒**，平滑结束后定格，不再切回站立待机；再次点击角色卡片或重新打开选人页会重播。镜头以腰部以上为主，随动作调整构图，为头饰、抬手动作和下方角色卡片留出空间；普通角色右侧显示武器选择，黄泉右侧保持空置。选人动作不改动战斗中的模型、跳跃动作或头像采样。

新入场资源与来源：

- 优菈：`pose/优菈待机pose_by_truthabout_*.zip` 和 `pose/eula-v3_by_fantong_*.zip`。保留原作者使用条件；没有将用户提供的动作标记为 CC0。VMD 按原 PMX 骨骼名称匹配、30 FPS 采样，转换包含身体和手指骨骼，不包含 MMD 物理模拟及表情 morph。UE 路径为 `/Game/Animations/Entrance/Eula/A_Eula_Eula_VMD_Entry`、`A_Eula_Eula_VMD_Finish`。
- 黄泉挥刀：[Quaternius Universal Animation Library](https://quaternius.com/packs/universalanimationlibrary.html)，CC0；使用本地 Standard 包的 `Sword_Attack`，资源为 `/Game/Animations/Entrance/Acheron/A_Acheron_Sword_Attack`。左手持鞘和收势时的手腕朝向另作适配，配刀网格为 `/Game/Characters/AcheronSword/SK_AcheronSword`。选人页保留刀与刀鞘的分开展示；实战将刀鞘套在刀上，双手握住同一刀柄，从右上蓄势向左下斜劈并回收；身体继续使用该动作资源，双臂按握点和斜劈轨迹适配。刀与刀鞘统一缩至资源原尺寸的 50%，便于第一人称展示。
- 阿斯卡纶：用户提供 `pose/Catwalk Sequence 03.fbx`，[Mixamo](https://www.mixamo.com/) 动作，遵循 Adobe Mixamo 条款，不是 CC0；资源为 `/Game/Animations/Entrance/Ascalon/A_Ascalon_Catwalk_Sequence_03`。
- 黄泉人物和配刀来自用户提供的模型压缩包，原文件注明 miHoYo 版权、流云景编辑，并限制商业使用及二次配布；阿斯卡纶沿用用户提供素材的原作者条件。此项不改变 Quaternius 动作包的独立 CC0 授权。

本地处理入口为 `Tools/inspect_roster_refresh.py`、`prepare_roster_refresh.py`、`prepare_eula_entrance.py` 和 `UnderTide/Scripts/import_roster_refresh.py`。只导入新角色及新入场，保留现有优菈、李织烟材质和手工头像。

李织烟抱猫展示参考用户提供视频的前四秒：<https://www.bilibili.com/video/BV1VtNH6wEXL/>。视频只作为动作参考，没有从中提取模型或骨骼动画。`BreachSelectionCat.cpp` 编排抬抱、低头轻靠和收稳三个阶段，猫朝向角色，前爪靠近肩膀；双手 IK 始终跟随猫的两个支撑点。猫的头、四肢、耳朵和尾巴使用自己的骨架，3.5 秒结束后与人物一起定格。猫仅在李织烟的选人预览中显示，切换角色或关闭页面时隐藏，不是战斗宠物或联机复制实体。

猫模型来自 Daily Lowpoly 的 [Lowpoly Cat + Run Animation](https://dailylowpoly.itch.io/lowpoly-cat-running)，作者明确允许用于商业项目；这是作者页面的许可，不是 CC0。源文件 `cat_rigged.fbx` 保留在本地 `SourceAssets/Converted/Cat/`，不作为独立素材包再分发。导入副本调整平滑法线，并配上白色材质、眼鼻和青色项圈；模型与参考视频中的猫不完全相同。

- 猫网格：`/Game/Characters/SelectionCat/SK_SelectionCat`，骨架：`/Game/Characters/SelectionCat/SK_SelectionCat_Skeleton`。
- 材质：`/Game/Characters/SelectionCat/M_CatFur`、`M_CatEye`、`M_CatPink`、`M_CatCollar`。
- 本地重建：`Tools/prepare_selection_cat.cpp`、`Tools/BuildSelectionCatTool.bat`、`UnderTide/Scripts/import_selection_cat.py`。猫的抱持动作由选人舞台编排，不加载源文件里的奔跑动画。

#### 滑铲动作

使用用户确认并提供的 [Mixamo Running Slide](https://www.mixamo.com/#/?page=1&query=running%20slide)，源文件为 `pose/Running Slide.fbx`（带蒙皮、30 FPS，动作长 1.533 秒）。授权依照 [Adobe Mixamo FAQ](https://helpx.adobe.com/creative-cloud/faq/mixamo-faq.html)，可免版税用于游戏，**不是 CC0**；原始动作不作为独立素材包再分发。

四个角色分别使用 `/Game/Animations/Locomotion/<Key>/A_<Key>_Running_Slide`，`<Key>` 为 `Eula`、`Acheron`、`Lizhiyan`、`Ascalon`。转换保留腿部和上身动作，去除水平根位移，并按各模型蒙皮后的鞋底、下腿和手部轮廓计算贴地高度；滑行速度与碰撞继续由移动组件控制。播放时加快滑入，并把贴地段延长到实际滑铲时长；结束后混合到当前下蹲或站姿，低矮空间内不会播放强制起身。

本地重建工具：`Tools/sample_mixamo_slide.cpp`、`Tools/BuildMixamoAnimationTool.bat`、`UnderTide/Scripts/retarget_mixamo_slide.py`；中间采样位于 `SourceAssets/Animations/MixamoSlide/Running_Slide.json`。工具和中间文件沿用项目的本地忽略规则。

#### 惯性滑铲和速度过渡

以 Apex 风格的惯性滑铲和滑铲跳为目标，按本项目的移动速度调校：地面实际速度达到 650 cm/s 时按 Ctrl 滑铲；WASD 可以缓慢改变滑行方向，松开 Ctrl 回到站姿。滑行按余速结束，不再被 1.5 秒定时器打断；下坡的重力分量会维持或提高速度，上坡会更快减速。滑铲中按 Space 可保留水平动量起跳，低矮天花板会阻止起跳；空中按住 Ctrl，落地速度足够时自动续滑。

滑入的额外 200 cm/s 加速分摊到 0.12 秒内，额外加速需要 1.25 秒恢复；恢复期间仍可进入滑行，但不会重复获得加速。滑铲加速的上限为 1200 cm/s。平地默认减速为 300 cm/s²，低于 240 cm/s 时结束；这是本项目的参数，不是 Apex 的精确数值。

滑铲时完整身体姿势会平滑贴合坡面，减少上坡时脚部穿地；第一人称相机仍使用独立的稳定位置，不跟随身体的坡度倾斜。

持枪、空手、黄泉持刀、瞄准和下蹲的目标速度分别为 510、790、870、300、200 cm/s，速度上限以每秒增加 1000、减少 1200 cm/s 的速率过渡，实际移动仍受加速度、制动和碰撞约束。滑铲退出从当前余速开始过渡；空中保留已有水平动量，落地后恢复地面过渡。

参数集中在 `BreachMovementComponent.h`。`SlidePoseDuration` 只影响滑铲动作从滑入到保持姿势的时间，不限制物理滑行时长。移动输入中的持枪/空手、瞄准意图纳入 UE 压缩移动标志，客户端重演保存速度过渡、滑铲和加速恢复状态；角色选择、武器及战斗状态的完整联机同步仍未完成。

### 不使用动画资源的程序化动作

以下行为由 C++、移动组件或程序化骨骼 IK 完成，没有单独的动画文件：

- 射击、枪口闪光、后坐力和射线命中。
- 持枪时双手对准枪械握把的 IK。
- 四名角色的第一人称常规视野统一为 110°，瞄准视野为 46°。
- 换弹计时、弹匣补充和空手近战状态；阿斯卡纶的直拳由程序化手臂 IK 生成。
- 下蹲碰撞体缩放、镜头高度平滑和墙体防穿透。
- 移动时的轻微武器摆动；角色镜头不会跟随动画产生大幅晃动。跳跃时第一人称双手固定在镜头下缘，且不复制世界模型的动态布料偏转；世界视角仍保留完整跳跃与布料动作。黄泉跑动时锁定持刀右手的位置，并将握点收在第一人称画面下缘，避免手掌脱离前臂单独露出；未攻击时本地带鞘刀不叠加步伐摆动，主动挥刀优先于跑动锁手和武器缓动，双手与刀柄同步；第一人称挥刀期间让左袖两条长布片垂向握点下方，并限制额外布料偏转，减少袖口遮挡刀路，世界模型保留布料物理、完整身体动作与双手斜劈轨迹。

黄泉是专属近战角色，不生成步枪状态。选择黄泉后会强制右手持带鞘的刀；左键播放约 0.72 秒的双手右上至左下斜劈，命中发生在动作约 42% 的落刀阶段，并以约 260 cm 的近战扫掠造成 180 点伤害。挥刀不额外绘制轨迹特效，按住左键可按攻击间隔连续攻击。主视角由正确映射到右手的世界刀实例提供刀影；复用同一复合网格的刀鞘实例不重复投射隐藏刀片，避免生成与人物分离的幽灵刀影。离开黄泉时同时关闭刀和刀鞘的可见性、普通投影及隐藏投影，其他角色持枪或空手时均不会残留刀影；重新选择黄泉后恢复有效武器的显示和投影。黄泉不能瞄准、换弹、收刀或切回步枪，离开黄泉时恢复进入黄泉前的其他角色武器状态。

优菈、李织烟和阿斯卡纶按 `3` 收枪后进入拳击近战状态，左键以约 145 cm 的近战扫掠挥拳并造成 70 点伤害，不消耗弹药。连续点击或按住左键时，右拳和左拳会逐拳交替；当前出拳从画面下方推进到准心对应一侧后收回，另一只手留在下方防守，确保主视角能清楚看到两侧拳路。两只手会按各角色实际掌宽与指骨长度收拢成拳，拇指横压拳面，避免长指甲或末节手指保持伸直。优菈和李织烟复用各自骨架的 `Female_Punch` 身体动作，并在手臂层镜像左右直拳；阿斯卡纶使用适配其骨架的同节奏程序化直拳。按 `1` 可恢复步枪。

### 输入动作

| 按键 | 输入动作 |
| --- | --- |
| `WASD` | 移动 |
| 鼠标移动 | 视角 |
| 鼠标左键 | 普通角色持枪时射击、收枪后挥拳；黄泉挥刀 |
| 鼠标右键 | 普通角色瞄准；黄泉无操作 |
| `1` | 普通角色恢复持枪；黄泉无操作 |
| `3` | 普通角色收起武器，进入可挥拳的快速移动状态；黄泉无操作 |
| `Space` | 跳跃；滑铲中保留水平动量起跳 |
| 按住 `Ctrl` | 低速下蹲；地面速度达到 650 cm/s 时按下触发滑铲；空中按住可在高速落地后续滑 |
| `R` | 普通角色换弹；黄泉无操作 |
| `H` | 打开或关闭选人页面 |
| `Esc` | 暂停；选人页面中返回游戏 |
| `Enter` | 重新开始；启动选人页面中开始游戏 |


### 目录中保留但当前未引用的旧资源

`/Game/Animations/Locomotion/<角色>/A_<角色>_Jump_Start`、`Jump_Loop`、`Jump_Land` 是之前的通用跳跃版本，仍保留在 Content 中作为备份；当前运行时加载的是上面的 `Female_Jump_Start`、`Female_Jump_Air`、`Female_Jump_Land`。
