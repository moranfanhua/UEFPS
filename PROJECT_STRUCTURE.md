# Neon Breach 项目结构说明

这份文档说明项目中每个目录的作用、游戏启动后的调用关系，以及修改功能时应该从哪里入手。项目是 Unreal Engine 5.7 的 C++ 第一人称竞技场原型，核心玩法是枪械、挥拳或黄泉挥刀近战、快速移动、击杀敌人、波次刷新和角色选择。

## 1. 先建立整体认识

新增表情和衣物模块：`BreachExpressions.cpp` 在编辑器中将原 PMX 顶点形变补入现有网格，并给优菈两段入场写入 VMD 表情曲线；`FBreachPose` 负责运行时曲线采样和渲染权重。`BreachCloth.h/.cpp` 保存各角色独立的衣物骨骼模拟状态，接在最终姿态与网格更新之间；玩家只模拟世界模型，再将衣物局部旋转应用到第一人称身体。`BreachDeformationTests.cpp` 由基础验证调用，选人验证另有表情对照截图。详细范围与限制见 README 的“VMD 表情和衣物物理”。

启动入口可以按下面的顺序理解：

```text
NeonBreach.uproject
        |
        v
Config/DefaultEngine.ini  -- 指定 Arena 地图和 BreachGameMode
        |
        v
ABreachGameMode::BeginPlay()
        |
        +--> BuildArena()             创建或检查竞技场
        +--> Spawn display enemies    创建后方四个角色展示台
        +--> ABreachCharacter         创建玩家、相机、枪/黄泉刀和双份人物模型
        +--> ABreachHUD                显示 HUD，并打开初始选人界面
        |
        v
玩家按 Enter 开始游戏
        |
        +--> 输入绑定 -> ABreachCharacter
        +--> 射线或近战扫掠命中 -> ABreachEnemy::TakeDamage()
        +--> 敌人倒地 -> ABreachGameMode::EnemyDefeated()
        +--> 波次结束 -> ABreachGameMode::StartWave()
```

当前是单机原型。代码大量通过 `UGameplayStatics::GetPlayerPawn(this, 0)` 访问一号玩家，后续做多人联机时，需要把波次、伤害、角色状态和生成逻辑逐步迁移到服务器权威流程。

## 2. 仓库目录

```text
GPT_UE_TEST/
├─ NeonBreach/
│  ├─ NeonBreach.uproject       UE 项目文件，EngineAssociation 为 5.7
│  ├─ Config/                   项目、输入、地图和渲染配置
│  ├─ Content/                  UE 二进制资源（uasset、umap）
│  └─ Source/
│     ├─ NeonBreach.Target.cs   游戏目标构建配置
│     ├─ NeonBreachEditor.Target.cs 编辑器目标构建配置
│     └─ NeonBreach/            C++ 游戏模块
├─ SourceAssets/                原始模型转换和骨骼审计产生的 JSON
├─ README.md                    项目简介和模型/动作清单
└─ PROJECT_STRUCTURE.md         本文档
```

`Binaries/`、`Intermediate/`、`Saved/` 等目录属于 UE 的构建或运行产物，通常由 `.gitignore` 忽略。测试截图和文本报告也会写到 `NeonBreach/Saved/`，它们不是游戏运行所必需的资源。

## 3. Source 目录：C++ 代码

### 3.1 模块和公共声明

| 文件 | 作用 |
| --- | --- |
| `NeonBreach/Source/NeonBreach/NeonBreach.cpp` | C++ 模块的最小启动文件，使用 `IMPLEMENT_PRIMARY_GAME_MODULE` 注册模块。 |
| `NeonBreach/Source/NeonBreach/NeonBreach.Build.cs` | 声明模块依赖：`Core`、`CoreUObject`、`Engine`、`InputCore`、`AIModule`、`NavigationSystem`、`SlateCore`；编辑器构建时额外使用网格、动画和 JSON 模块。 |
| `NeonBreach/Source/NeonBreach/BreachGame.h` | 主要类的统一头文件，声明玩家控制器、玩家角色、敌人、GameMode 和 HUD。虽然类很多，但实现分别放在不同 `.cpp` 文件中。 |
| `NeonBreach/Source/NeonBreach/BreachVisuals.h` | `Breach` 命名空间的资源入口：四个角色的 Key/显示名、角色网格加载、材质加载和激光束绘制。 |
| `NeonBreach/Source/NeonBreach/CharacterRigData.h` | 四个角色的身体和手指骨骼映射表，由骨骼名称生成工具产生。 |
| `NeonBreach/Source/NeonBreach/CharacterBones.h` | PMX 模型中肩膀、手臂、腿和脊柱等关键骨骼的兼容名称表。 |

角色 Key 的顺序固定为：

```text
0 Eula       -> 优菈
1 Acheron    -> 黄泉
2 Lizhiyan   -> 李织烟
3 Ascalon    -> 阿斯卡纶
```

这个顺序同时影响角色资源路径、选人卡片、动画路径和敌人模型。添加角色时，不能只增加一个模型文件，还要同步更新 `BreachVisuals.h`、骨骼映射和对应动画资源。

### 3.2 游戏流程和波次

| 文件 | 主要职责 |
| --- | --- |
| `BreachGameMode.cpp` | 游戏总流程。启动时调用 `BuildArena()`，创建展示角色；运行时管理波次、分数、击杀数、敌人数量、胜负状态和自动演示。 |
| `BreachArena.cpp` | 创建竞技场几何、地板、围墙、掩体、中央反应堆、角色展示台、文字标牌、灯光和后处理。已有带 `BreachArena` 标签的物体时不会重复创建。 |
| `BreachEnemy.cpp` | 敌人移动、朝向玩家、视线检测、攻击、受伤和死亡。普通移动由 `Pose.Walk()` 程序化生成，死亡优先播放对应的 `Death01` 动画。 |
| `BreachAssets.cpp` | 编辑器资源处理：裁剪辅助第一人称手臂网格，以及从 JSON/动作文件烘焙死亡和角色动画。函数使用 `WITH_EDITOR`，打包后的游戏不会执行编辑器写入操作。 |
| `BreachModelReview.cpp` | 阿斯卡纶正、侧、背离屏检查入口；只有显式使用 `-BreachModelReview` 才进入模型检查场景，附加 `-BreachReviewHead` 聚焦头部。`Scripts/VerifyModelReview.ps1 -Head` 封装该入口。保留原骨架的 `CopyCharacterGeometry` 位于 `BreachAssets.cpp`，网格更新仅允许编辑器构建执行。 |
| `BreachCharacter.cpp` | 玩家角色的构造、输入绑定、相机、完整人物模型、枪械组件、黄泉刀、射击/挥拳/挥刀、瞄准、换弹、受伤和重开。普通角色收枪后以 70 点伤害挥拳，黄泉固定持刀并以 180 点伤害攻击。 |
| `BreachLocomotion.cpp` | 玩家移动状态和动画切换：待机、持枪移动、空手或黄泉持刀奔跑、起跳、空中、落地、下蹲和滑铲。滑铲使用四个角色各自的 Mixamo `Running_Slide` 资源，按实际滑行时长播放贴地段；状态切换使用短时间骨骼混合，减少动作跳变。 |
| `BreachMovementComponent.h/.cpp` | 自定义角色移动组件：统一处理持枪/空手/黄泉持刀/瞄准/下蹲速度过渡，黄泉持刀速度为 870 cm/s；处理 Ctrl 惯性滑铲、有限转向、坡道加减速、滑铲跳和落地续滑，并保存客户端移动重演状态。 |

`ABreachGameMode` 的主要运行数据如下：

```text
Wave              当前波次
Score             分数
Kills             击杀数
EnemiesAlive      场上未被击败的敌人数量
RemainingToSpawn  当前波次尚未生成的敌人数量
bGameOver         玩家是否死亡/本局结束
bGallery          是否处于展示或测试模式
```

每波敌人数为 `4 + Wave * 2`，同时场上最多保持 8 个敌人。敌人被击败后，GameMode 更新分数、弹药和少量生命值；最后一个敌人倒地且没有待生成敌人时，进入下一波间歇。

### 3.3 骨骼姿势和动画采样

| 文件 | 作用 |
| --- | --- |
| `BreachPose.h/.cpp` | 自定义的轻量骨骼姿势系统。负责建立参考姿势、组件空间重建、骨骼旋转、手臂 IK、手指握持、程序化走路、`AnimSequence` 采样以及应用到 `UPoseableMeshComponent`。 |
| `EBreachBone` | 抽象出 Pelvis、Spine、Chest、Neck、Head、双臂、双手、双腿和双脚等 17 个关键部位，避免业务代码直接依赖某一个模型的骨骼编号。 |
| `EBreachLocomotion` | 玩家移动状态枚举，顺序必须与 `BreachLocomotion.cpp` 中的动画数组一致。 |

四个人物的骨架名称不同，`FBreachPose::Init()` 先通过 `CharacterRigData.h` 找到每个角色的实际骨骼，再让后续代码只使用 `EBreachBone`。因此修改腿部方向、手臂握枪或死亡姿态时，优先检查骨骼映射和 `BreachPose`，不要在每个角色的逻辑里复制一套旋转代码。

### 3.4 选人界面

| 文件 | 作用 |
| --- | --- |
| `BreachSelectionStage.h/.cpp` | 场景中的选人预览演员。播放优菈 VMD 拼接、黄泉挥刀、李织烟抱猫、阿斯卡纶 Catwalk；3.5 秒后保持姿势。动态头像捕获接口保留，当前 UI 使用用户提供的 `Portraits/T_<Key>_Portrait` 静态贴图。 |
| `BreachSelectionSword.cpp` | 黄泉专属刀、鞘加载与双手绑定，左手持鞘 IK、收势角度适配和仅选人可见的管理。 |
| `BreachLocalAnimation.cpp` | 编辑器 VMD 骨骼轨道烘焙入口，按优菈已有骨架写入姿势序列，避免重导入用户调整过的模型和贴图。 |
| `BreachSelectionCat.cpp` | 同一选人舞台的拆分实现。为李织烟创建带骨骼的白猫，编排抱持和低头轻靠，双手 IK 跟随猫的支撑点；结束后人物与猫一同定格，离开预览时隐藏。 |
| `BreachSelectionHUD.cpp` | 绘制选人界面、标题、角色卡片、按钮和提示文字；处理鼠标 HitBox；打开界面时切换相机、暂停游戏、显示鼠标，关闭时恢复原来的视角和暂停状态。 |
| `BreachHUD.cpp` | 游戏内 HUD：生命值、弹药、波次、准星、命中提示、通知和操作提示。 |
| `ABreachPlayerController` | 目前只有一个用于选人暂停期间继续 Tick 的小接口，方便预览动画在暂停世界时更新。 |

选人界面的状态关系是：

```text
bStartupPending
    └─ 首次拥有 Pawn 后自动打开初始选人界面

bInitialSelection
    └─ 初始界面显示“开始游戏”，Enter/H/Esc 可开始

bSelectionOpen
    └─ H 打开或关闭；鼠标点击角色卡片调用 ChooseOperator()
```

选人预览被放在竞技场远处的隐藏位置，并由 `ABreachSelectionStage` 自己的相机观察，因此不会干扰实际战斗场景。

### 3.5 测试代码

| 文件 | 作用 |
| --- | --- |
| `BreachMovementTests.cpp` | 自动按键测试奔跑、跳跃、下蹲、速度过渡、滑铲跳/落地续滑、上下坡、低矮天花板、墙体防穿透、恢复持枪和镜头稳定性，并输出截图和报告。 |
| `BreachSelectionTests.cpp` | 测试初始选人、暂停状态、四个角色点击、3–4 秒入场时长、上半身构图、结尾姿势持续保持、Enter 开始游戏、H 返回选人和 F1-F4 不再切换角色；保存 Entrance / Mid / Finish / Hold 截图。 |
| `BreachGameMode.cpp::RunSmokeTest()` | 快速检查四个模型、第一人称/世界模型、头部隐藏、手部握枪、死亡资源和敌人倒地状态。 |

常用命令行参数：

```text
-BreachTest             基础烟雾测试
-BreachMovementTest     移动、跳跃、下蹲和镜头测试
-BreachSelectionTest    选人流程测试
-BreachGallery          进入展示模式
-BreachAutoPlay         自动射击和移动演示
-BreachCapture          延时截图并退出
```

测试产生的结果一般位于 `NeonBreach/Saved/`，例如 `selection_test.txt`、`MovementFPS_*.txt` 和截图文件。

## 4. Content 目录：UE 资源

```text
NeonBreach/Content/
├─ Animations/
│  ├─ Death/                 四个角色各自的 Death01
│  ├─ Entrance/              选人入场和待机动作
│  └─ Locomotion/            每个角色的移动、奔跑、跳跃、下蹲和滑铲动作
├─ Audio/                    Confirm、Fire、Reload 音效
├─ Characters/
│  ├─ Eula/
│  ├─ Acheron/
│  ├─ Lizhiyan/
│  ├─ Ascalon/
│  ├─ AcheronSword/          黄泉原模型附带的刀与鞘
│  ├─ Portraits/             用户修改的四张静态头像贴图
│  └─ SelectionCat/           李织烟选人预览的猫模型、骨架和材质
├─ Maps/Arena.umap           编辑器启动地图和游戏默认地图
└─ Materials/                角色、枪械、竞技场和霓虹灯材质
```

### 4.1 资源命名和 C++ 加载方式

代码使用固定的 UE 路径约定加载资源：

```text
/Game/Characters/<Key>/SK_<Key>                  角色 Skeletal Mesh
/Game/Animations/Locomotion/<Key>/A_<Key>_<Clip> 移动动画
/Game/Animations/Death/A_<Key>_Death01            死亡动画
/Game/Animations/Entrance/<Key>/A_<Key>_<Clip>    选人动作
/Game/Materials/<Name>                            材质
/Game/Audio/<Name>                                音效
```

例如 `Eula` 的待机动画完整路径是：

```text
/Game/Animations/Locomotion/Eula/A_Eula_Idle_Loop.A_Eula_Idle_Loop
```

重命名 `.uasset` 时必须同时修改 C++ 中拼出的路径，或者保留当前命名格式。

### 4.2 第一人称模型的两份实例

玩家不是只显示手臂，而是同时维护两个完整人物模型：

```text
ABreachCharacter
├─ Body       仅本地玩家可见；隐藏头部，避免相机进入脸部
└─ WorldBody  本地玩家不可见；保留完整头部和身体，投射世界影子
```

两份模型使用同一个 `SK_<Key>`，由 `FBreachPose` 同步姿势。枪械也有 `WeaponRoot` 和 `WorldWeaponRoot` 两套组件。普通角色按 `3` 收起武器时，两套枪械都隐藏并关闭世界枪械的阴影，同时左键切换为 70 点伤害的挥拳；按 `1` 恢复持枪。

## 5. Config 目录

| 文件 | 关键内容 |
| --- | --- |
| `DefaultEngine.ini` | 默认地图 `/Game/Maps/Arena`、全局 GameMode `BreachGameMode`、DX12、抗锯齿、阴影、帧率和导航网格设置。 |
| `DefaultGame.ini` | 项目名称、版本、打包配置，以及始终烘焙的 `Characters`、`Materials`、`Audio`、`Animations` 目录。 |
| `DefaultInput.ini` | 传统输入系统的 Action/Axis 映射。当前移动使用 WASD，视角使用鼠标，动作键包括 1、3、Space、Ctrl、R、H、Esc 和 Enter。 |
| `DefaultEditor.ini` | 编辑器预览场景的共享灯光和后处理预设。 |

输入绑定最终在 `ABreachCharacter::SetupPlayerInputComponent()` 中连接到 C++ 方法：

| 输入 | C++ 处理 |
| --- | --- |
| `MoveForward` / `MoveRight` | `MoveForward()` / `MoveRight()` |
| `Turn` / `LookUp` | `Turn()` / `LookUp()` |
| `Fire` | `StartFire()`、`StopFire()`、`Fire()` |
| `Aim` | `SetAim()` |
| `Reload` | `Reload()` |
| `Jump` | `ACharacter::Jump()` / `StopJumping()` |
| `Unarmed`（3） | `HolsterRifle()` |
| `DrawRifle`（1） | `DrawRifle()` |
| `Crouch`（Ctrl） | `CrouchOn()` / `CrouchOff()` |
| `Pause`（Esc） | `TogglePause()` |
| `Selection`（H） | `ToggleSelection()` |
| `Restart`（Enter） | `RestartRun()` |

F1-F4 已经不在 `DefaultInput.ini` 的角色切换逻辑中；角色切换应该通过选人界面完成。

## 6. SourceAssets 目录

```text
SourceAssets/
├─ character_rig_mapping.json  四个角色关键骨骼和手指映射
├─ conversion_report.json       模型转换、骨骼、材质和纹理审计结果
└─ marionette_fbx_audit.json    旧木偶模型的历史审计，不参与当前角色资源加载
```

这些 JSON 是导入和排查模型问题时的参考资料，不是运行时必须加载的游戏资源。`CharacterRigData.h` 是从骨骼映射生成到 C++ 中的运行时版本。

## 7. 修改功能时从哪里开始

| 想修改的内容 | 建议先看 |
| --- | --- |
| 角色外观、材质、披风 | `Content/Characters/<Key>`、`Content/Materials`、`BreachCharacter.cpp` 和 `BreachEnemy.cpp` 中的材质槽处理。 |
| 角色选择界面 | `BreachSelectionHUD.cpp`、`BreachSelectionStage.cpp`。 |
| 移动、跳跃、奔跑、下蹲 | `BreachCharacter.cpp` 的输入和速度，`BreachLocomotion.cpp` 的状态，`Content/Animations/Locomotion/<Key>` 的资源。 |
| 枪械、射击、挥拳、挥刀、瞄准、换弹 | `BreachCharacter.cpp` 的 `Fire()`、`PerformPunchHit()`、`PerformSwordHit()`、`Reload()`、`SetAim()`。 |
| 敌人行为和死亡 | `BreachEnemy.cpp`，以及 `Content/Animations/Death`。 |
| 波次、分数和生成点 | `BreachGameMode.cpp` 的 `StartWave()`、`SpawnEnemy()`、`EnemyDefeated()`。 |
| 骨骼方向、握枪、IK | `CharacterRigData.h`、`CharacterBones.h`、`BreachPose.cpp`。 |
| 竞技场灯光和掩体 | `BreachArena.cpp::BuildArena()`、`Content/Materials`。 |
| 输入按键 | `Config/DefaultInput.ini` 和 `BreachCharacter::SetupPlayerInputComponent()`，两边需要保持一致。 |

推荐的阅读顺序是：`BreachGame.h` → `BreachGameMode.cpp` → `BreachCharacter.cpp` → `BreachLocomotion.cpp` → `BreachSelectionHUD.cpp` / `BreachSelectionStage.cpp` → `BreachPose.cpp`。
