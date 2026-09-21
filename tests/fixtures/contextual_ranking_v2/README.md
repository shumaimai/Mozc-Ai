# Contextual Ranking v2 canonical fixtures

The machine-readable fixture is maintained in the training repository at
`tests/fixtures/contextual_ranking_v2/cases.jsonl` and is copied into runtime
tests/reviews without model data.  It covers context cleaning, reading
normalization, short/numeric/punctuation inputs, and a multi-segment target.

The expected formatter is the v2 contract:

```text
読み: <reading>\n文脈: <cleaned_context>\n候補: <candidate>
```

Raw preceding text is test input; cleaned context is the only model context.
