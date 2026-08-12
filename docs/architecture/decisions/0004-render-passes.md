# ADR 0004: Backend-neutral ordered render passes

Status: accepted for milestone 0.13

Rendering is organized as clear, opaque, translucent, edge/line, selection,
and overlay passes. The renderer owns ordering while the private graphics device
implements backend operations. Public pass, mapper, actor, and window contracts
do not expose OpenGL object names or shader implementation details.
