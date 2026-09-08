# Neon Breach

正在试图学习UE，明年能找到实习吗...

好焦虑好焦虑好焦虑好崩溃好崩溃好难好难好难怎么这么难

---

模型和动作全是网上搞的，使用清单后面再写吧，被搞不完的问题搞崩溃了

---

## 模型与动作清单

以下是项目当前实际使用的角色模型、动画资源和输入动作。四个角色均使用完整人物模型；第一人称视角只隐藏本地视角中的头部，世界模型仍保留完整身体并投射影子，方便后续多人联机。

### 角色模型

| 角色 | UE Skeletal Mesh | 原始模型文件 | 用途 |
| --- | --- | --- | --- |
| 优菈 | `/Game/Characters/Eula/SK_Eula` | `SourceAssets/Eula/优菈.pmx` | 玩家、敌人展示、选人页面 |
| 联动优菈 | `/Game/Characters/EulaCasual/SK_EulaCasual` | `SourceAssets/EulaCasual/优菈.pmx` | 玩家、敌人展示、选人页面 |
| 李织烟 | `/Game/Characters/Lizhiyan/SK_Lizhiyan` | `SourceAssets/Lizhiyan/李织烟.pmx` | 玩家、敌人展示、选人页面 |
| Marionette | `/Game/Characters/Marionette/SK_Marionette` | Marionette FBX 模型 | 玩家、敌人展示、选人页面 |

每个角色还带有对应的 Skeleton、材质、纹理、Physics Asset，以及 `SK_*_FPArms` 资源。当前第一人称实现使用上表中的完整 `SK_*` 模型，`FPArms` 仅作为导入后保留的辅助资源。

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
| `A_EulaCasual_Death01` | 联动优菈敌人 |
| `A_Lizhiyan_Death01` | 李织烟敌人 |
| `A_Marionette_Death01` | Marionette 敌人 |

敌人被击杀后播放对应的 `Death01` 倒地动作，之后保留倒地模型一段时间再清理；四个角色分别使用与自身骨骼匹配的资源。

#### 选人页面入场和待机

资源目录：`/Game/Animations/Entrance/<角色>/`。每个角色都有以下五条适配后的动画，共 20 个 UE 动画资源。

| 源动作 | 选人页面中的作用 |
| --- | --- |
| `Female_Standing` | 起身站定 |
| `Female_Clapping` | 拍手亮相 |
| `Female_Punch` | 出拳亮相 |
| `Female_Jump` | 轻跳亮相 |
| `Female_Idle` | 入场动作结束后的循环待机 |

当前角色与入场动作的对应关系：

| 角色 | 选中后播放的入场动作 | 结束后 |
| --- | --- | --- |
| 优菈 | `Female_Standing` | `Female_Idle` |
| 联动优菈 | `Female_Clapping` | `Female_Idle` |
| 李织烟 | `Female_Punch` | `Female_Idle` |
| Marionette | `Female_Jump` | `Female_Idle` |

#### 滑铲动作

使用用户确认并提供的 [Mixamo Running Slide](https://www.mixamo.com/#/?page=1&query=running%20slide)，源文件为根目录的 `Running Slide.fbx`（带蒙皮、30 FPS，动作长 1.533 秒）。授权依照 [Adobe Mixamo FAQ](https://helpx.adobe.com/creative-cloud/faq/mixamo-faq.html)，可免版税用于游戏，**不是 CC0**；原始动作不作为独立素材包再分发。

四个角色分别使用 `/Game/Animations/Locomotion/<Key>/A_<Key>_Running_Slide`，`<Key>` 为 `Eula`、`EulaCasual`、`Lizhiyan`、`Marionette`。转换保留腿部和上身动作，去除水平根位移，并按各模型蒙皮后的鞋底、下腿和手部轮廓计算贴地高度；滑行速度与碰撞继续由移动组件控制。播放时加快滑入，并把贴地段延长到实际滑铲时长；结束后混合到当前下蹲或站姿，低矮空间内不会播放强制起身。

本地重建工具：`Tools/sample_mixamo_slide.cpp`、`Tools/BuildMixamoAnimationTool.bat`、`NeonBreach/Scripts/retarget_mixamo_slide.py`；中间采样位于 `SourceAssets/Animations/MixamoSlide/Running_Slide.json`。工具和中间文件沿用项目的本地忽略规则。

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
| `Space` | 跳跃 |
| 按住 `Ctrl` | 低速下蹲；地面速度达到 650 cm/s 时按下触发滑铲 |
| `R` | 换弹 |
| `H` | 打开或关闭选人页面 |
| `Esc` | 暂停；选人页面中返回游戏 |
| `Enter` | 重新开始；启动选人页面中开始游戏 |


### 目录中保留但当前未引用的旧资源

`/Game/Animations/Locomotion/<角色>/A_<角色>_Jump_Start`、`Jump_Loop`、`Jump_Land` 是之前的通用跳跃版本，仍保留在 Content 中作为备份；当前运行时加载的是上面的 `Female_Jump_Start`、`Female_Jump_Air`、`Female_Jump_Land`。
