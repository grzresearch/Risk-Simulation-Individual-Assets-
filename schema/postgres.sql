-- macro_risk Postgres schema
-- Persists each scan run with full JSON payload + extracted columns for
-- quick filtering. Compatible with vanilla Postgres 13+ (no extensions
-- required).

CREATE TABLE IF NOT EXISTS macro_runs (
    run_id      TEXT PRIMARY KEY,
    run_date    DATE NOT NULL,
    version     TEXT NOT NULL,
    git_sha     TEXT NOT NULL,
    payload     JSONB NOT NULL,
    created_at  TIMESTAMPTZ DEFAULT now()
);

CREATE INDEX IF NOT EXISTS idx_macro_runs_date    ON macro_runs (run_date DESC);
CREATE INDEX IF NOT EXISTS idx_macro_runs_payload ON macro_runs USING gin (payload jsonb_path_ops);

-- Optional: append-only audit table for SIEM ingestion when not using files.
CREATE TABLE IF NOT EXISTS macro_audit (
    id         BIGSERIAL PRIMARY KEY,
    ts         TIMESTAMPTZ NOT NULL DEFAULT now(),
    actor      TEXT NOT NULL,
    action     TEXT NOT NULL,
    version    TEXT NOT NULL,
    git_sha    TEXT NOT NULL,
    fields     JSONB
);

CREATE INDEX IF NOT EXISTS idx_macro_audit_ts     ON macro_audit (ts DESC);
CREATE INDEX IF NOT EXISTS idx_macro_audit_actor  ON macro_audit (actor, action);
