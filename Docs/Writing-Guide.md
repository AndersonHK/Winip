# Documentation style

Write for a contributor who has not read the development conversation. Begin with
the document's purpose and current state. Use direct, concrete prose and meaningful
headings. Prefer active voice, explain necessary technical terms, and remove
session narration, personal preferences and obsolete instructions from maintained
guides. Historical summaries must identify their date and limits.

## One authority per topic

[The documentation map](README.md) names each authoritative guide. Define a rule
in that guide and link to it elsewhere rather than copying it. A quick-start
example can repeat a command when it helps the reader, but it must agree with the
authoritative instructions. Keep requirements, implementation details, plans and
historical outcomes distinct.

Use precise status language: **implemented**, **observed**, **proposed**, or
**unverified**. Building an installer does not establish installation success;
a screenshot does not establish execution support; a passing fixture is not a
performance guarantee. Document meaningful limitations beside the claim.

## Paths, examples and references

- Use repository-relative paths with exact on-disk casing, and fenced PowerShell commands for Windows examples. Run examples from the repository root unless stated otherwise.
- Use synthetic filenames and placeholders for external inputs. Never copy profile paths, private notebook names, machine identities, tokens or private outputs into public docs.
- Link to primary upstream sources and state why they apply, their limits, and what future decision should revisit them. Maintain the catalogue in [References.md](References.md).
- Keep upstream license attributions intact; those names and contacts are required provenance, not maintainer PII.
- Use tables for comparisons, lists for parallel items or steps, and descriptive link labels. Avoid unexplained abbreviations and giant inventories in introductory documents.

## Updating and archiving

Update the guide that owns changed behavior, then update links/status in the
roadmap and parity tracker as needed. Archive useful superseded designs under
`Docs/Archive/`, clearly labeled as historical. Raw logs, captured images, JSON
reports and compiled products follow [Validation.md](Validation.md)'s local-only
retention policy; do not archive them into Git. Check relative links and examples
before completing a documentation change.
