# Spire of Roco 🃏⚔️

《Spire of Roco》是一款结合了《杀戮尖塔》（Slay the Spire）的Roguelike卡牌构建玩法，与《洛克王国》（Roco Kingdom）的三精灵小队、属性克制及状态BUFF机制的【双人回合制卡牌对战游戏】。

## 核心玩法机制
- **三角色小队队伍 (Team of 3)**：每位玩家控制由3名具有不同属性（水、火、草、电、普通）的角色组成的小队，并可以进行角色轮换。
- **卡牌构筑 (Deck-building)**：围绕手牌中的能量（Energy），巧妙组合使用攻击（Attack）、技能（Skill）、能力（Power）三种类别卡牌。
- **状态与 Buff 系统**：支持烧伤、中毒、潮湿、护盾与强化等多元状态效果，用以改变战局。
- **博弈决策 (Turn-Based)**：每回合需在出牌打击、切换上场角色、保留能量与结束回合之间做出策略抉择。

## 技术架构
本项目采用**严格多级解耦**的架构设计与网络通讯分离模式：

- **基座合约**：依托于唯一的全局约束文件 `game_core.h` 提供所有的变量、枚举和结构体支持，作为唯一的跨端数据字典合约。
- **C语言核心引擎 (Engine & Core Logic)**：
  - **语言/编译**：C 语言开发，MSVC / GCC 编译。
  - **职责**：维护核心战斗回合状态机（GameState）、卡牌数值管理读取法则与规则校验。
  - **外壳渲染**：使用 **EasyX** 图形库提供原生本地轻量级的客户端窗体视图与游戏呈现。
  
- **Python AI 后端 (AI Agent)**：
  - **职责**：提供启发式、LLM 与混合式 AI 决策，应对玩家操作。
  - **通信框架**：不借助底层FFI绑定，完全通过 **本地 TCP Sockets** 与 C 引擎通信，C端向其发送 `GameState`，Python端回传行动 JSON。
  - **当前能力**：已支持合法动作生成、难度分层、LLM `action_id` 约束、批量评测、选角/构筑规划、对手建模与 AI 决策日志。

## 核心目录划分 📁
- `src_data/`：游戏静、动态数值层。包含数值解析 (`cards.txt` / `characters.txt`) 以及独立伤害加成演算模块。
- `src_engine/`：管理生命周期、游戏驱动进程环与校验机。
- `src_gui/`：接管渲染线程，包含基于 EasyX 图形库渲染界面的 API 集合封装。
- `src_ai/`：AI 模块。包含 C 侧 TCP Bridge、Python AI 后端、合法动作生成、LLM/启发式策略、评测脚本与构筑规划工具。

## AI 分支当前用法

启动本地 AI 后端：

```bash
ROCO_AI_POLICY=hard python3 -B src_ai/ai_backend/main.py
```

运行无 EasyX 的 PvE 冒烟：

```bash
ROCO_GAME_MODE=1 ROCO_SMOKE_MAX_ROUNDS=1 ./run_smoketest_without_EasyX.sh
```

运行 AI 评测：

```bash
python3 -B src_ai/ai_backend/evaluate.py --matches 20 --p1-policy random --p2-policy hard
```

生成 AI 选角/构筑推荐：

```bash
python3 -B src_ai/ai_backend/draft_planner.py --style aggressive --deck-size 16
```

可用 AI 策略包括：

- `easy`：随机合法动作。
- `heuristic` / `normal`：普通启发式评分。
- `hard`：增强启发式评分，带轻量前瞻与对手画像修正。
- `llm`：调用 OpenAI-compatible / Qwen 后端，从合法 `action_id` 中选择。
- `hybrid`：先由 `hard` 生成候选，再让 LLM 从候选 `action_id` 中选择。

AI 决策日志默认写入 `logs/ai_decisions_YYYYMMDD.jsonl`，`logs/` 已被 git 忽略。
