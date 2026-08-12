# Naming and ABI Rules

## Names

```c
typedef struct FVizPolyData FVizPolyData;
FVizPolyData* fviz_poly_data_create(void);
#define FVIZ_CELL_TRIANGLE 5
```

## Public structs

Small value types may be public structs. Long-lived object types should use opaque declarations so internal representation can evolve without breaking ABI.

## Ownership vocabulary

Future API documentation must identify returned/passed references as one of:

- borrowed: caller must not release it;
- retained/owned: caller must release it;
- transferred: ownership moves to the callee.

## ABI version

`FVIZ_ABI_VERSION` is independent from the semantic library version. Breaking binary changes require an ABI version increment.
