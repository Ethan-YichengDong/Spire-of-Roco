# 🤖 洛克王国爬塔版 (Spire of Roco) - Agent AI 开发约束协议

> **📝 致组员与AI助手 (To Team Members & AI Agents):**
> 无论是阅读代码还是生成新的卡牌、机制、被动效果代码，**所有 AI Agent 必须严格遵守本文档中的结构和约束规范**。禁止自行猜测或捏造未经定义的变量格式与结算逻辑！

---

## 🛑 第一铁律：绝不绕过三段式结算流水线 (Damage Pipeline)
所有的卡牌伤害计算、元素反应、扣血阵亡等，必须严格通过 `src_data/battle_calculator.c` 中的三段管道计算，**禁止直接在逻辑中粗暴写出 `target->hp -= damage;` 的代码**！

当需要实现伤害卡牌时，你的代码片段必须通过以下流转：
1. **`GetRawDamage(card, attacker)`**：结算基础伤害（附加攻击力buff等）。
2. **`CalculateMitigation(raw_damage, card, target)`**：处理元素克制（水克火等）、异常状态Combo（例如感电）、护盾抵扣、减伤。
3. **`CommitDamageAndCheck(final_damage, target)`**：实际执行真实扣血，并在这内部触发角色阵亡（`is_alive = 0`）等事件钩子。

---

## 🎯 第二铁律：严格遵守目标索引 (Targeting) 规约
`Action` 结构体中的 `target_idx` 用于指代由于技能/攻击产生的具体目标，禁止 Agent 混淆敌我索引。请强制按照以下映射编写目标解析逻辑：

* **单体敌方目标 (**`0` ~ `2`**)**：对应对方 `target_player->team[target_idx]`（例如：0 号位为对方首发）。
* **单体己方目标 (**`10` ~ `12`**)**：对应己方 `acting_player->team[target_idx - 10]`（用于回血、上护盾技能）。
* **AOE 敌方全体 (**`-1`**)**：要求使用 `for(int i=0; i<TEAM_SIZE; i++)` 遍历对方依然存活 (`is_alive == 1`) 的角色。
* **AOE 己方全体 (**`-2`**)**：要求遍历己方队伍存活成员。

---

## 🔧 第三铁律：状态与阶段枚举 (Enums & States) 的显式化
禁止在判断条件中使用魔法数字（Magic Numbers，例如 `if(type == 1)` 或 `if(game_stage == 2)`）。必须使用 `game_core.h` 中已定义的规范宏与枚举：

* **元素分类**：`ELEMENT_NORMAL`, `ELEMENT_WATER`, `ELEMENT_FIRE`, `ELEMENT_GRASS`, `ELEMENT_ELECTRIC`
* **卡面类型**：`CARD_TYPE_ATTACK`, `CARD_TYPE_SKILL`, `CARD_TYPE_POWER`
* **异常状态**：`BUFF_WET`, `BUFF_SHIELD`, `BUFF_WET`, `BUFF_BURN`, `BUFF_POISON`, `BUFF_POWER`
* **场景节点**：`SCENE_MENU`, `SCENE_DRAFT`, `SCENE_BATTLE`, `SCENE_RESULT`

**💡 如果人类用户让你新增一个“冰冻”或者“深渊”阶段，AI 必须第一件事就是前往 `game_core.h` 去扩充对应的 `enum`，然后再编写逻辑。**

---

## 📊 第四铁律：Buff 回合数与资源限定
* **关于 Buff 扣减 (`buffs[BUFF_COUNT]`)**：
  数组中存放的 `int` 值代表**“可触发回合数/持续时间”**（例如 `target->buffs[BUFF_BURN] = 3` 代表燃烧 3 回合）。禁止将其认定为层数而进行受击频次扣减。
* **关于能量 (`energy`)**：
  在恢复/扣除玩家能量时，除非用户指明为“扩展能量上限”类型卡，否则**操作后必须调用钳制逻辑**（如 `min(player->energy, player->max_energy)` 和 `max(player->energy, 0)`），保证 UI 不会产生越界崩溃。

---

## 📂 模块落点规范 (Where to Put Code)
请 Agent 严格遵照项目MVC模型指派文件去处，不可随意写在不同的文件里：
1. **纯数值增减、伤害计算、特殊卡逻辑分配** ➡️ 填入 `src_data/battle_calculator.c`。
2. **洗牌、卡池操作、外存读取、掉落获取** ➡️ 填入 `src_data/data_manager.c`。
3. **主界面的UI绘制、警告弹窗、颜色、终端提示** ➡️ 填入 `src_gui/gui_manager.cpp` / `mac_cli_manager.cpp`。
4. **游戏流程循环控制、玩家/AI回合轮询** ➡️ 填入 `src_engine/game_engine.c`。