"""Regression tests for the Pawn hot-path guard and benchmark comparison."""
import unittest

from tools.check_pawn_complexity import scan, loops_of, timer_roots
from tools.check_pawn_static import mask_opaque
from tools.run_pawn_benchmark import compare, parse_run, summarize


def warnings(source: str) -> set[str]:
    return {w.rule for w in scan({"gamemodes/test.pwn": source})}


class ComplexityTests(unittest.TestCase):
    def test_nested_mass_scan_on_hot_timer(self):
        src = '''
#define PERIOD (1000)
forward Fast();
public OnGameModeInit() { SetTimer("Fast", PERIOD, true); return 1; }
public Fast()
{
    for (new p = 0; p < MAX_PLAYERS; p++)
    {
        for (new b = 0; b < MAX_BUSINESSES; b++)
        {
            UpdateBusiness(p,b);
        }
    }
    return 1;
}
'''
        self.assertEqual(timer_roots({"x.pwn":src})["Fast"], 1000)
        self.assertIn("NESTED_MASS_SCAN", warnings(src))

    def test_nested_loops_outside_hot_paths_not_reported(self):
        src = '''
stock Inert()
{
    for (new p; p < MAX_PLAYERS; p++) {
        for (new b; b < MAX_BUSINESSES; b++) UpdateBusiness(p,b);
    }
    return 1;
}
'''
        self.assertNotIn("NESTED_MASS_SCAN", warnings(src))

    def test_heavy_io_inside_tick_helper(self):
        src = '''
public OnPlayerUpdate(playerid)
{
    return ProcessPlayers();
}
stock ProcessPlayers()
{
    for (new p; p < MAX_PLAYERS; p++)
    {
        mysql_query(db,"SELECT * FROM accounts");
    }
    return 1;
}
'''
        self.assertIn("HOT_LOOP_IO", warnings(src))

    def test_ignores_comments_and_quoted_code(self):
        src = '''
public OnPlayerUpdate(playerid)
{
    // for (int i = 0; i < MAX_PLAYERS; i++) mysql_query("hi");
    new message[100];
    format(message,sizeof(message), "while(i) { fopen }");
    return 1;
}
'''
        self.assertEqual(warnings(src), set())
        self.assertEqual(loops_of(mask_opaque(src)), [])

    def test_timer_interval_and_frequent_loop(self):
        src = '''
public OnGameModeInit() { SetTimer("Work", 200, true); return 1; }
public Work()
{
    for (new i; i < 50; i++) {}
    for (new j; j < 50; j++) {}
    return 1;
}
'''
        self.assertIn("FAST_TIMER_LOOPS", warnings(src))

    def test_no_false_periodic_timer_for_one_shot(self):
        src = '''
public OnGameModeInit() { SetTimer("Work", 200, false); return 1; }
public Work()
{
    for (new i; i < 50; i++) {}
    for (new j; j < 50; j++) {}
    return 1;
}
'''
        self.assertEqual(warnings(src), set())


class BenchmarkLogicTests(unittest.TestCase):
    def test_parse_complete(self):
        lines = []
        for i in range(7):
            lines += [
                f"PERF_SAMPLE:name=distance,pass={i},ms={10+i},checksum=100",
                f"PERF_SAMPLE:name=voyage,pass={i},ms={20+i},checksum=200",
            ]
        lines.append("PERF_DONE:cases=2,samples=7")
        result = parse_run("\n".join(lines))
        self.assertEqual(result["distance"]["median_ms"], 13)
        self.assertEqual(result["voyage"]["checksum"], 200)

    def test_missing_or_duplicate_sample_is_error(self):
        with self.assertRaises(ValueError):
            parse_run("PERF_DONE:cases=2,samples=7")
        text = "\n".join(
            [f"PERF_SAMPLE:name=distance,pass={i},ms=10,checksum=1" for i in range(7)]
            + [f"PERF_SAMPLE:name=voyage,pass={i},ms=20,checksum=2" for i in range(7)]
            + ["PERF_SAMPLE:name=distance,pass=0,ms=10,checksum=1",
               "PERF_DONE:cases=2,samples=7"]
        )
        with self.assertRaises(ValueError):
            parse_run(text)

    def test_noise_gate_and_regression(self):
        base = {**summarize([20]*7), "checksum": 1}
        slow = {**summarize([26]*7), "checksum": 1}
        self.assertEqual(compare(base, slow)["status"], "regression")
        self.assertEqual(compare(base, slow, identical_source=True)["status"], "unchanged")
        self.assertEqual(compare(base, {**slow,"checksum":2})["status"], "incomparable")
        self.assertEqual(compare(base, {**summarize([21]*7),"checksum":1})["status"], "ok")
        self.assertEqual(compare({**summarize([1]*7),"checksum":1}, slow)["status"], "noisy")
        volatile = {**summarize([10,11,12,80,90,100,110]), "checksum":1}
        self.assertTrue(volatile["noisy"])
        self.assertEqual(compare(volatile, slow)["status"], "noisy")


if __name__ == "__main__":
    unittest.main()
