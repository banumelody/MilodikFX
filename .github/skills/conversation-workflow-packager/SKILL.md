---
name: conversation-workflow-packager
description: "Create a reusable SKILL.md from chat history. Use when converting a repeated process into a project skill with decision points, quality checks, and example prompts."
argument-hint: "What recurring workflow should this skill package?"
user-invocable: true
disable-model-invocation: false
---

# Conversation Workflow Packager

## What This Skill Produces

This skill produces a complete `SKILL.md` draft that turns a repeated chat workflow into a reusable, project-scoped skill.

## When to Use

- You notice the same step-by-step method repeated across requests.
- You want to preserve review or implementation checklists as a slash-invocable workflow.
- You need a first version quickly, then refine ambiguous decision points.

## Procedure

1. Review the current chat and extract:

- Ordered steps that are repeated
- Decision points with branching logic
- Quality gates that define completion

2. If the workflow is not clear, infer a minimal default flow and mark assumptions.
3. Create a workspace skill at `.github/skills/<skill-name>/SKILL.md`.
4. Write frontmatter with:

- `name` that matches folder name
- A keyword-rich `description` with "use when" triggers
- Optional `argument-hint` if slash use benefits from input guidance

5. Write the body with:

- Outcome and scope
- Trigger conditions
- Step-by-step process
- Decision rules
- Completion checks
- Example prompts

6. Identify weak or ambiguous areas and ask focused follow-up questions.
7. Incorporate answers and finalize with a short "how to use" section.

## Decision Rules

- If workflow appears project-specific, store under `.github/skills/`.
- If workflow is personal and cross-project, recommend a personal skills directory.
- If the process is single-shot and parameterized, suggest a prompt file instead of a skill.
- If the process must be always-on, suggest instructions instead of a skill.

## Completion Checks

- `name` matches folder name exactly.
- Description contains concrete discovery keywords.
- Steps are executable without hidden knowledge.
- Decision points are explicit.
- At least 3 example prompts are included.

## Example Prompts

- `/conversation-workflow-packager Turn our bug triage routine into a reusable skill.`
- `/conversation-workflow-packager Package our release checklist into a team skill.`
- `/conversation-workflow-packager Convert our code review method into a slash command workflow.`

## Follow-on Customizations

- Add a paired prompt in `.github/prompts/` for one-off skill drafting.
- Add instructions in `.github/instructions/` for naming and frontmatter conventions.
- Add a custom agent if this becomes a multi-file customization pipeline.
