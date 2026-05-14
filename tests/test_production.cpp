#include "alerts/slack_alerter.h"
#include "governance/audit_log.h"
#include "governance/model_card.h"
#include "governance/rbac.h"
#include "persist/store.h"
#include "report/csv_reporter.h"
#include "report/html_reporter.h"
#include "report/reporter.h"
#include "server/metrics.h"
#include "util/thread_pool.h"
#include "util/version.h"

#include <atomic>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

static int g_fails = 0;
static void check(bool c, const char* m) {
    if (!c) { std::cerr << "FAIL " << m << "\n"; ++g_fails; }
}

int main() {
    using namespace macro;
    namespace fs = std::filesystem;

    // -- Version --------------------------------------------------------
    check(!version().empty(), "version non-empty");

    // -- ThreadPool -----------------------------------------------------
    {
        util::ThreadPool pool(4);
        std::atomic<int> ctr{0};
        std::vector<std::future<int>> fs;
        for (int i = 0; i < 100; ++i) {
            fs.push_back(pool.submit([&ctr]() { ++ctr; return 1; }));
        }
        int sum = 0;
        for (auto& f : fs) sum += f.get();
        check(sum == 100, "pool 100 tasks");
        check(ctr.load() == 100, "pool counter");
    }

    // -- Metrics --------------------------------------------------------
    {
        auto& m = server::Metrics::instance();
        m.inc("test_total");
        m.inc("test_total", 4.0);
        m.set("test_gauge", 42.0);
        auto out = m.expose();
        check(out.find("test_total") != std::string::npos, "metrics counter exposed");
        check(out.find("test_gauge") != std::string::npos, "metrics gauge exposed");
    }

    // -- Persistence: JSON store ----------------------------------------
    {
        auto dir = fs::temp_directory_path() / "macro_runs_test";
        fs::remove_all(dir);
        auto store = persist::make_json_store(dir.string());
        check(static_cast<bool>(store), "json store ctor");
        persist::StoredRun r;
        r.run_id = "20260514T100000Z";
        r.run_date = "2026-05-14";
        r.version = version();
        r.git_sha = git_sha();
        r.json_payload = "{\"hello\":\"world\"}";
        store->save(r);
        auto loaded = store->load(r.run_id);
        check(loaded.has_value(), "json store roundtrip");
        check(loaded->run_id == r.run_id, "json store run_id");
        auto ids = store->list_runs(10);
        check(!ids.empty(), "json store list");
    }

    // -- Audit log ------------------------------------------------------
    {
        auto path = (fs::temp_directory_path() / "macro_audit_test.log").string();
        std::remove(path.c_str());
        governance::AuditLog audit(path);
        audit.record("alice", "scan.run", {{"assets", "5"}});
        audit.record("bob",   "report.export", {{"format", "csv"}});
        std::ifstream f(path);
        std::string contents((std::istreambuf_iterator<char>(f)),
                             std::istreambuf_iterator<char>());
        check(contents.find("\"actor\":\"alice\"") != std::string::npos, "audit alice");
        check(contents.find("\"action\":\"scan.run\"") != std::string::npos, "audit action");
        check(contents.find("\"format\":\"csv\"") != std::string::npos, "audit fields");
    }

    // -- RBAC -----------------------------------------------------------
    {
        auto rb = governance::Rbac::default_roles();
        check( rb.has_permission("admin",   governance::Permission::ManageConfig), "admin manage");
        check(!rb.has_permission("viewer",  governance::Permission::ManageConfig), "viewer no manage");
        check( rb.has_permission("analyst", governance::Permission::Export),       "analyst export");
        check(!rb.has_permission("nobody",  governance::Permission::ReadReports),  "no role denied");
    }

    // -- Alert rule evaluation -----------------------------------------
    {
        engine::ScanOutput so;
        AssetReport ar;
        ar.asset.id = "spy";
        ScenarioResult sc;
        sc.event.kind = EventKind::CPI;
        sc.event.date = time::today_utc();
        sc.forecast.prob_up = 0.78;
        sc.forecast.weighted_prob_up = 0.80;
        sc.forecast.bh_q = 0.02;
        sc.forecast.sample_size = 30;
        sc.regime = "Hiking";
        ar.scenarios.push_back(sc);
        so.assets.push_back(ar);

        alerts::AlertRule rule;
        auto items = alerts::evaluate(so, rule);
        check(items.size() == 1, "alert produced");
        check(items[0].asset_id == "spy", "alert asset");
    }

    // -- Reports: CSV + HTML produce non-empty output -------------------
    {
        engine::ScanOutput so;
        AssetReport ar;
        ar.asset.id = "spy";
        ar.asset.display_name = "SPY";
        ar.asset.asset_class = AssetClass::ETF;
        ar.spot_price = 500.0;
        ar.hist_vol_annualized = 0.15;
        ScenarioResult sc;
        sc.event.kind = EventKind::CPI;
        sc.event.date = time::today_utc();
        sc.forecast.sample_size = 40;
        sc.forecast.prob_up = 0.6;
        sc.forecast.mean_return = 0.003;
        ar.scenarios.push_back(sc);
        so.assets.push_back(ar);

        std::ostringstream csv;
        report::to_csv(csv, so);
        check(csv.str().find("spy,") != std::string::npos, "csv has row");

        std::ostringstream html;
        report::to_html(html, so);
        check(html.str().find("<table") != std::string::npos, "html has table");

        std::ostringstream mc;
        Config cfg;
        cfg.horizon_days = 90;
        governance::write_model_card(mc, cfg, so);
        check(mc.str().find("Model Card") != std::string::npos, "model card heading");
    }

    if (g_fails == 0) std::cout << "OK\n";
    return g_fails == 0 ? 0 : 1;
}
