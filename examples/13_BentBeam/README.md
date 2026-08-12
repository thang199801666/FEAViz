# Bent HEX8 cantilever beam

`FEAVizBentBeam` generates a structured 32×4×4 hexahedral cantilever mesh, applies an analytical Euler–Bernoulli bending displacement field, and displays the deformed result colored by Von Mises stress with the Rainbow preset.

```text
out\build\windows-msvc-debug\bin\FEAVizBentBeam.exe
```

The generated model contains 825 points, 512 HEX8 elements, 1,088 surface triangles, and 1,088 visible boundary-grid segments. The fixed-end outer fibers reach 250 MPa and the center of the free end deflects by 2.5 model units.

For automated verification without a render window:

```text
out\build\windows-msvc-debug\bin\FEAVizBentBeam.exe --validate
```
