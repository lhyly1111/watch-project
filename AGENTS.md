# OV-Watch Reference Snapshot

`项目源码` is an immutable local reference snapshot for OV-Watch V2.4.3. It contains copied release firmware, the original archive, and the extracted upstream source.

- Do not edit, format, generate into, build in, test in, move, delete, or commit any file under `项目源码`.
- The source is for read-only comparison and diagnosis. Make future implementation changes only in a separately created replication or application workspace.
- Verify any claim about this reference snapshot against the local files, not a similarly named remote release.
- The tracked study document is `OV-Watch-V2.4.3-源码研读报告.md` at this repository root. Update it when a substantive new static analysis corrects or extends its conclusions.

`RTOS` remains a separate STM32F411 Keil + MDK learning project. Do not inspect or modify it unless the user explicitly requests RTOS work.

## Daily GitHub learning snapshot

The active implementation is `app1/OV_Watch_APP2`. When the user asks for a daily OV-Watch learning summary, include that day's verified APP2 code and configuration in a dedicated Git commit, append the day's summary, verification state, unresolved risks, and next action to the repository root `README.md`, and push the commit to `https://github.com/lhyly1111/watch-project.git`. Exclude build outputs, IDE metadata, `tmp/`, the `RTOS/` learning project, and the immutable `项目源码/` reference snapshot.
