# Neon Breach

正在试图学习UE，明年能找到实习吗...

好焦虑好焦虑好焦虑好崩溃好崩溃好难好难好难怎么这么难

---

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

四张手工头像位于 `/Game/Characters/Portraits/T_<Key>_Portrait`，选人页保留用户修改的竖版卡片，战斗 HUD 使用同一套贴图。更换角色资源不会重新生成或覆盖这些头像。

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
| `A_<角色>_Jog_Fwd_Loop` | 持枪前进 | 持枪移动 |
| `A_<角色>_Sprint_Loop` | 空手奔跑 | 按 `3` 收起武器后移动 |
| `A_<角色>_Crouch_Idle_Loop` | 下蹲待机 | 按住 `Ctrl` 且不移动 |
| `A_<角色>_Crouch_Fwd_Loop` | 下蹲移动 | 按住 `Ctrl` 移动 |
| `A_<角色>_Female_Jump_Start` | 女性化起跳、离地、收腿 | 按 `Space` 起跳 |
| `A_<角色>_Female_Jump_Air` | 空中收膝到伸腿 | 跳跃上升和下降阶段 |
| `A_<角色>_Female_Jump_Land` | 伸腿落地、站稳 | 角色接触地面 |

跳跃动作来自同一条女性动作 `HumanArmature|Female_Jump`，在项目中拆成了 `Start / Air / Land` 三段。`Female_Jump_Air` 会根据实际垂直速度从收腿过渡到伸腿。

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

四个入场展示统一为 **3.5 秒**，平滑结束后定格，不再切回站立待机；再次点击角色卡片或重新打开选人页会重播。镜头以腰部以上为主，随动作调整构图，为头饰、抬手动作和下方角色卡片留出空间，右侧继续空置。选人动作不改动战斗中的模型、跳跃动作或头像采样。

新入场资源与来源：

- 优菈：`pose/优菈待机pose_by_truthabout_*.zip` 和 `pose/eula-v3_by_fantong_*.zip`。保留原作者使用条件；没有将用户提供的动作标记为 CC0。VMD 按原 PMX 骨骼名称匹配、30 FPS 采样，转换包含身体和手指骨骼，不包含 MMD 物理模拟及表情 morph。UE 路径为 `/Game/Animations/Entrance/Eula/A_Eula_Eula_VMD_Entry`、`A_Eula_Eula_VMD_Finish`。
- 黄泉挥刀：[Quaternius Universal Animation Library](https://quaternius.com/packs/universalanimationlibrary.html)，CC0；使用本地 Standard 包的 `Sword_Attack`，资源为 `/Game/Animations/Entrance/Acheron/A_Acheron_Sword_Attack`。左手持鞘和收势时的手腕朝向另作适配，配刀网格为 `/Game/Characters/AcheronSword/SK_AcheronSword`。刀与鞘只在黄泉选人展示中出现。
- 阿斯卡纶：用户提供 `pose/Catwalk Sequence 03.fbx`，[Mixamo](https://www.mixamo.com/) 动作，遵循 Adobe Mixamo 条款，不是 CC0；资源为 `/Game/Animations/Entrance/Ascalon/A_Ascalon_Catwalk_Sequence_03`。
- 黄泉人物和配刀来自用户提供的模型压缩包，原文件注明 miHoYo 版权、流云景编辑，并限制商业使用及二次配布；阿斯卡纶沿用用户提供素材的原作者条件。此项不改变 Quaternius 动作包的独立 CC0 授权。

本地处理入口为 `Tools/inspect_roster_refresh.py`、`prepare_roster_refresh.py`、`prepare_eula_entrance.py` 和 `NeonBreach/Scripts/import_roster_refresh.py`。只导入新角色及新入场，保留现有优菈、李织烟材质和手工头像。

李织烟抱猫展示参考用户提供视频的前四秒：<https://www.bilibili.com/video/BV1VtNH6wEXL/>。视频只作为动作参考，没有从中提取模型或骨骼动画。`BreachSelectionCat.cpp` 编排抬抱、低头轻靠和收稳三个阶段，猫朝向角色，前爪靠近肩膀；双手 IK 始终跟随猫的两个支撑点。猫的头、四肢、耳朵和尾巴使用自己的骨架，3.5 秒结束后与人物一起定格。猫仅在李织烟的选人预览中显示，切换角色或关闭页面时隐藏，不是战斗宠物或联机复制实体。

猫模型来自 Daily Lowpoly 的 [Lowpoly Cat + Run Animation](https://dailylowpoly.itch.io/lowpoly-cat-running)，作者明确允许用于商业项目；这是作者页面的许可，不是 CC0。源文件 `cat_rigged.fbx` 保留在本地 `SourceAssets/Converted/Cat/`，不作为独立素材包再分发。导入副本调整平滑法线，并配上白色材质、眼鼻和青色项圈；模型与参考视频中的猫不完全相同。

- 猫网格：`/Game/Characters/SelectionCat/SK_SelectionCat`，骨架：`/Game/Characters/SelectionCat/SK_SelectionCat_Skeleton`。
- 材质：`/Game/Characters/SelectionCat/M_CatFur`、`M_CatEye`、`M_CatPink`、`M_CatCollar`。
- 本地重建：`Tools/prepare_selection_cat.cpp`、`Tools/BuildSelectionCatTool.bat`、`NeonBreach/Scripts/import_selection_cat.py`。猫的抱持动作由选人舞台编排，不加载源文件里的奔跑动画。

#### 滑铲动作

使用用户确认并提供的 [Mixamo Running Slide](https://www.mixamo.com/#/?page=1&query=running%20slide)，源文件为 `pose/Running Slide.fbx`（带蒙皮、30 FPS，动作长 1.533 秒）。授权依照 [Adobe Mixamo FAQ](https://helpx.adobe.com/creative-cloud/faq/mixamo-faq.html)，可免版税用于游戏，**不是 CC0**；原始动作不作为独立素材包再分发。

四个角色分别使用 `/Game/Animations/Locomotion/<Key>/A_<Key>_Running_Slide`，`<Key>` 为 `Eula`、`Acheron`、`Lizhiyan`、`Ascalon`。转换保留腿部和上身动作，去除水平根位移，并按各模型蒙皮后的鞋底、下腿和手部轮廓计算贴地高度；滑行速度与碰撞继续由移动组件控制。播放时加快滑入，并把贴地段延长到实际滑铲时长；结束后混合到当前下蹲或站姿，低矮空间内不会播放强制起身。

本地重建工具：`Tools/sample_mixamo_slide.cpp`、`Tools/BuildMixamoAnimationTool.bat`、`NeonBreach/Scripts/retarget_mixamo_slide.py`；中间采样位于 `SourceAssets/Animations/MixamoSlide/Running_Slide.json`。工具和中间文件沿用项目的本地忽略规则。

#### 惯性滑铲和速度过渡

以 Apex 风格的惯性滑铲和滑铲跳为目标，按本项目的移动速度调校：地面实际速度达到 650 cm/s 时按 Ctrl 滑铲；WASD 可以缓慢改变滑行方向，松开 Ctrl 回到站姿。滑行按余速结束，不再被 1.5 秒定时器打断；下坡的重力分量会维持或提高速度，上坡会更快减速。滑铲中按 Space 可保留水平动量起跳，低矮天花板会阻止起跳；空中按住 Ctrl，落地速度足够时自动续滑。

滑入的额外 200 cm/s 加速分摊到 0.12 秒内，额外加速需要 1.25 秒恢复；恢复期间仍可进入滑行，但不会重复获得加速。滑铲加速的上限为 1200 cm/s。平地默认减速为 300 cm/s²，低于 240 cm/s 时结束；这是本项目的参数，不是 Apex 的精确数值。

滑铲时完整身体姿势会平滑贴合坡面，减少上坡时脚部穿地；第一人称相机仍使用独立的稳定位置，不跟随身体的坡度倾斜。

持枪、空手、瞄准和下蹲的目标速度分别为 510、790、300、200 cm/s，速度上限以每秒增加 1000、减少 1200 cm/s 的速率过渡，实际移动仍受加速度、制动和碰撞约束。滑铲退出从当前余速开始过渡；空中保留已有水平动量，落地后恢复地面过渡。

参数集中在 `BreachMovementComponent.h`。`SlidePoseDuration` 只影响滑铲动作从滑入到保持姿势的时间，不限制物理滑行时长。移动输入中的持枪/空手、瞄准意图纳入 UE 压缩移动标志，客户端重演保存速度过渡、滑铲和加速恢复状态；角色选择、武器及战斗状态的完整联机同步仍未完成。

### 不使用动画资源的程序化动作

以下行为由 C++、移动组件或程序化骨骼 IK 完成，没有单独的动画文件：

- 射击、枪口闪光、后坐力和射线命中。
- 持枪时双手对准枪械握把的 IK。
- 瞄准时武器位置和视野变化。
- 换弹计时、弹匣补充和空手状态。
- 下蹲碰撞体缩放、镜头高度平滑和墙体防穿透。
- 移动时的轻微武器摆动；角色镜头不会跟随动画产生大幅晃动。

### 输入动作

| 按键 | 输入动作 |
| --- | --- |
| `WASD` | 移动 |
| 鼠标移动 | 视角 |
| 鼠标左键 | 射击 |
| 鼠标右键 | 瞄准 |
| `1` | 恢复持枪 |
| `3` | 收起武器，空手移动时奔跑 |
| `Space` | 跳跃；滑铲中保留水平动量起跳 |
| 按住 `Ctrl` | 低速下蹲；地面速度达到 650 cm/s 时按下触发滑铲；空中按住可在高速落地后续滑 |
| `R` | 换弹 |
| `H` | 打开或关闭选人页面 |
| `Esc` | 暂停；选人页面中返回游戏 |
| `Enter` | 重新开始；启动选人页面中开始游戏 |


### 目录中保留但当前未引用的旧资源

`/Game/Animations/Locomotion/<角色>/A_<角色>_Jump_Start`、`Jump_Loop`、`Jump_Land` 是之前的通用跳跃版本，仍保留在 Content 中作为备份；当前运行时加载的是上面的 `Female_Jump_Start`、`Female_Jump_Air`、`Female_Jump_Land`。
