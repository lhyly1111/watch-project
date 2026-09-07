# OV-Watch Reference Snapshot

`项目源码` is an immutable local reference snapshot for OV-Watch V2.4.3. It contains copied release firmware, the original archive, and the extracted upstream source.

- Do not edit, format, generate into, build in, test in, move, delete, or commit any file under `项目源码`.
- The source is for read-only comparison and diagnosis. Make future implementation changes only in a separately created replication or application workspace.
- Verify any claim about this reference snapshot against the local files, not a similarly named remote release.
- The tracked study document is `OV-Watch-V2.4.3-源码研读报告.md` at this repository root. Update it when a substantive new static analysis corrects or extends its conclusions.

`RTOS` remains a separate STM32F411 Keil + MDK learning project. Do not inspect or modify it unless the user explicitly requests RTOS work.

## APP2 hardware diagnosis

When an APP2 hardware feature compiles and downloads but does not behave on the board, first confirm execution with a breakpoint or debugger state. Then compare the relevant APP2 CubeMX `.ioc`, generated `main.h`/GPIO/SPI configuration, and the immutable V2.4.3 reference driver. List the concrete differences and change only one hardware or protocol variable per test. Treat successful board behavior as the final evidence, and record the configuration that produced it.

## APP2 development roadmap

For every OV-Watch question or APP2 change, first read `C:\Users\Administrator\Desktop\Obsidian\OV_Watch\开发路线\项目学习报告与事实基线.md`, then the high-level roadmap `C:\Users\Administrator\Desktop\Obsidian\OV_Watch\开发路线\OV Watch APP2 开发目录.md`, and the relevant detailed stage note under `开发路线\阶段详解`; also read the current source, `.ioc`, schematic, or reference needed for the question. Keep the high-level roadmap high-level. Before starting a roadmap stage, create or extend that stage's linked Obsidian note with the detailed implementation and learning plan; after completion, update that note with verification evidence, risks and the next action, then update roadmap status if it changed. Do not advance to a dependent stage until its stated acceptance condition is met. The old `C:\Users\Administrator\Desktop\手表项目\OV-Watch-学习对话记录.md` is historical only and must not receive new entries.

For APP2 learning code, add a nearby Chinese explanation for every introduced configuration constant, buffer, state variable, pointer, and loop variable. It must state what it represents, unit/range or layout when applicable, why its current value exists, who reads or writes it, and its lifetime or replacement plan when relevant. For every HAL, CMSIS, FreeRTOS, LVGL, or other library call used in learning code, explain the function's role at that call site, the meaning of its important arguments, ordering or blocking behavior, and what success or failure does and does not prove. A useful function-level comment also explains the component's responsibility, input/output meaning, data or signal flow, ordering constraints, design reason, and the failure mode or common misconception it prevents. Keep temporary test code, production BSP code, and later LVGL/DMA ownership boundaries explicit in comments.

This detailed Chinese teaching-comment convention applies to the majority of APP2 code written or substantively changed for the user, especially BSP, protocol, task, hardware-configuration, and test code, until the user explicitly asks to stop or reduce it. Do not rewrite third-party libraries or CubeMX-generated code merely to add comments; explain those dependencies at the APP2 call sites instead.

## Daily Obsidian learning notes

The active implementation is `app1/OV_Watch_APP2`. When the user asks for a daily OV-Watch learning summary, create or update a dated Markdown note in `C:\Users\Administrator\Desktop\Obsidian\OV_Watch\每日总结` and update the relevant relation chain in `C:\Users\Administrator\Desktop\Obsidian\OV_Watch\关系链`. Each note must distinguish verified results, unresolved risks, and the next action, and should link to its relation chain and relevant protocol notes.

Do not append daily learning summaries to this repository's `README.md`, and do not upload the Obsidian daily notes to GitHub. When the user asks for a daily summary, push that day's verified APP2 code and configuration commits, but never the summary, README, or Obsidian files. For an ordinary code push request, likewise push only APP2 code/configuration commits. Exclude build outputs, IDE metadata, `tmp/`, the `RTOS/` learning project, and the immutable `项目源码/` reference snapshot.
