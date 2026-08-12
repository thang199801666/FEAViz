# ADR 0005: Cooperative cancellation across pipeline and parallel work

Status: accepted for milestones 0.10 and 0.12

Cancellation is cooperative and represented by an atomic token carried by the
pipeline request and parallel task group. Algorithms check it at bounded work
intervals. Cancellation preserves previously valid cached output, never
publishes partial replacement output, and reports one deterministic root result.
