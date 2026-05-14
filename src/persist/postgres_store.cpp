#include "persist/store.h"

#include "util/logger.h"

#ifdef MACRO_HAVE_LIBPQ
#  include <libpq-fe.h>
#endif

#include <memory>
#include <stdexcept>

namespace macro::persist {

#ifdef MACRO_HAVE_LIBPQ

namespace {

struct ConnDeleter { void operator()(PGconn* c) const { if (c) PQfinish(c); } };
struct ResDeleter  { void operator()(PGresult* r) const { if (r) PQclear(r);  } };
using ConnPtr = std::unique_ptr<PGconn, ConnDeleter>;
using ResPtr  = std::unique_ptr<PGresult, ResDeleter>;

class PostgresStore final : public Store {
public:
    explicit PostgresStore(const std::string& dsn) {
        conn_.reset(PQconnectdb(dsn.c_str()));
        if (!conn_ || PQstatus(conn_.get()) != CONNECTION_OK) {
            throw std::runtime_error("postgres connect failed: " +
                std::string(conn_ ? PQerrorMessage(conn_.get()) : "null"));
        }
        ensure_schema();
    }

    void save(const StoredRun& run) override {
        const char* params[] = {
            run.run_id.c_str(), run.run_date.c_str(),
            run.version.c_str(), run.git_sha.c_str(),
            run.json_payload.c_str()};
        ResPtr r(PQexecParams(conn_.get(),
            "INSERT INTO macro_runs (run_id, run_date, version, git_sha, payload) "
            "VALUES ($1, $2, $3, $4, $5::jsonb) "
            "ON CONFLICT (run_id) DO UPDATE SET payload = EXCLUDED.payload",
            5, nullptr, params, nullptr, nullptr, 0));
        if (PQresultStatus(r.get()) != PGRES_COMMAND_OK) {
            throw std::runtime_error(std::string("postgres save: ") + PQerrorMessage(conn_.get()));
        }
    }

    std::optional<StoredRun> load(const std::string& run_id) const override {
        const char* params[] = {run_id.c_str()};
        ResPtr r(PQexecParams(conn_.get(),
            "SELECT run_id, run_date::text, version, git_sha, payload::text "
            "FROM macro_runs WHERE run_id = $1",
            1, nullptr, params, nullptr, nullptr, 0));
        if (PQresultStatus(r.get()) != PGRES_TUPLES_OK) return std::nullopt;
        if (PQntuples(r.get()) == 0) return std::nullopt;
        StoredRun s;
        s.run_id       = PQgetvalue(r.get(), 0, 0);
        s.run_date     = PQgetvalue(r.get(), 0, 1);
        s.version      = PQgetvalue(r.get(), 0, 2);
        s.git_sha      = PQgetvalue(r.get(), 0, 3);
        s.json_payload = PQgetvalue(r.get(), 0, 4);
        return s;
    }

    std::vector<std::string> list_runs(int limit) const override {
        std::vector<std::string> ids;
        std::string sql = "SELECT run_id FROM macro_runs ORDER BY run_date DESC LIMIT " +
                          std::to_string(limit);
        ResPtr r(PQexec(conn_.get(), sql.c_str()));
        if (PQresultStatus(r.get()) != PGRES_TUPLES_OK) return ids;
        for (int i = 0; i < PQntuples(r.get()); ++i) ids.emplace_back(PQgetvalue(r.get(), i, 0));
        return ids;
    }

    std::string kind() const override { return "postgres"; }

private:
    void ensure_schema() {
        ResPtr r(PQexec(conn_.get(),
            "CREATE TABLE IF NOT EXISTS macro_runs ("
            " run_id   TEXT PRIMARY KEY,"
            " run_date DATE NOT NULL,"
            " version  TEXT NOT NULL,"
            " git_sha  TEXT NOT NULL,"
            " payload  JSONB NOT NULL,"
            " created_at TIMESTAMPTZ DEFAULT now()"
            ")"));
        if (PQresultStatus(r.get()) != PGRES_COMMAND_OK) {
            throw std::runtime_error(std::string("postgres schema: ") + PQerrorMessage(conn_.get()));
        }
    }

    ConnPtr conn_;
};

} // namespace

std::unique_ptr<Store> make_postgres_store(const std::string& dsn) {
    try {
        return std::make_unique<PostgresStore>(dsn);
    } catch (const std::exception& e) {
        log::error(std::string("postgres init failed: ") + e.what());
        return nullptr;
    }
}

#else

std::unique_ptr<Store> make_postgres_store(const std::string& /*dsn*/) {
    log::warn("Built without libpq; postgres backend unavailable.");
    return nullptr;
}

#endif

} // namespace macro::persist
