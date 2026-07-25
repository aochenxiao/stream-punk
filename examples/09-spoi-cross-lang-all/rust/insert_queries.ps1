$path = "d:\dev\project\aochenxiao\stream-punk\examples\09-spoi-cross-lang-all\rust\client.rs"
$content = [System.IO.File]::ReadAllText($path)
$marker = 'println!("=== 所有查询完成 ===");'
$insert = @'

    // ================================================================
    //  进阶查询 L8-L12：难度逐级上升，刁钻组合验证库正确性
    // ================================================================
    println!("\n========== 高阶查询 L8-L12 ==========\n");

    // ---- 等级 8: 管道操作边缘情况 ----
    test_num += 1;
    println!("--- 查询 {}: 【L8】REVERSE x2 — 应与原始顺序相同 ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players().reverse().reverse().build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    test_num += 1;
    println!("--- 查询 {}: 【L8】TAKE(0) — 取0个元素（空向量） ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players().take(0).build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    test_num += 1;
    println!("--- 查询 {}: 【L8】DROP(0) — 丢弃0个（应返回全部） ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players().drop(0).build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    test_num += 1;
    println!("--- 查询 {}: 【L8】SORT 覆盖 — SORT(level,asc) + SORT(hp,desc) ---", test_num);
    println!("    (以最后一次排序为准)");
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players().sort(PLAYER_LEVEL, true).sort(PLAYER_HP, false).build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    test_num += 1;
    println!("--- 查询 {}: 【L8】REVERSE x3 — 等同于单次 REVERSE ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players().reverse().reverse().reverse().build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    test_num += 1;
    println!("--- 查询 {}: 【L8】DROP 到只剩 1 个 + TAKE(1) ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players().drop(4).take(1).build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    // ---- 等级 9: 数值边界与极端值 ----
    test_num += 1;
    println!("--- 查询 {}: 【L9】FILTER hp < 0 — 无玩家 hp 为负 ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players().filter(PLAYER_HP, CMP_LT, 0).build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    test_num += 1;
    println!("--- 查询 {}: 【L9】SET hp=0, FILTER hp EQ 0 — 零值精确匹配 ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.set(&[0, 0, PLAYER_HP], 0).from_players().filter(PLAYER_HP, CMP_EQ, 0).build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    test_num += 1;
    println!("--- 查询 {}: 【L9】ADD 负值使金币变负, FILTER gold < 0 ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.add(&[0, 4, PLAYER_GOLD], -200).from_players().filter(PLAYER_GOLD, CMP_LT, 0).build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    test_num += 1;
    println!("--- 查询 {}: 【L9】互斥条件 — FILTER hp>0, FILTER hp<=0（必然空） ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players().filter(PLAYER_HP, CMP_GT, 0).filter(PLAYER_HP, CMP_LE, 0).build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    test_num += 1;
    println!("--- 查询 {}: 【L9】FILTER level = 0 — 不存在 level=0 的玩家 ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players().filter(PLAYER_LEVEL, CMP_EQ, 0).build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    test_num += 1;
    println!("--- 查询 {}: 【L9】FILTER hp >= 0（全部通过） + COUNT ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players().filter(PLAYER_HP, CMP_GE, 0).count().build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    // ---- 等级 10: 写操作与管道混合 ----
    test_num += 1;
    println!("--- 查询 {}: 【L10】多次 SET 后管道查询 — 改 3 个玩家 hp，然后 FILTER + SORT ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.set(&[0, 0, PLAYER_HP], 10)
            .set(&[0, 1, PLAYER_HP], 20)
            .set(&[0, 2, PLAYER_HP], 30)
            .from_players()
            .filter(PLAYER_HP, CMP_GT, 15)
            .sort(PLAYER_HP, true)
            .build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    test_num += 1;
    println!("--- 查询 {}: 【L10】SET + ADD 同一字段后查询 ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.set(&[0, 0, PLAYER_GOLD], 100)
            .add(&[0, 0, PLAYER_GOLD], 50)
            .from_players()
            .find_str(PLAYER_NAME, "Alice")
            .build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    test_num += 1;
    println!("--- 查询 {}: 【L10】ADD 全部玩家 level+1, 然后 FILTER + COUNT ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.add(&[0, 0, PLAYER_LEVEL], 1)
            .add(&[0, 1, PLAYER_LEVEL], 1)
            .add(&[0, 2, PLAYER_LEVEL], 1)
            .add(&[0, 3, PLAYER_LEVEL], 1)
            .add(&[0, 4, PLAYER_LEVEL], 1)
            .from_players()
            .filter(PLAYER_LEVEL, CMP_GT, 5)
            .count()
            .build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    test_num += 1;
    println!("--- 查询 {}: 【L10】SET 不存在索引 [0,99] — 应静默忽略，无玩家 hp>9000 ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.set(&[0, 99, PLAYER_HP], 9999)
            .from_players()
            .filter(PLAYER_HP, CMP_GT, 9000)
            .build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    test_num += 1;
    println!("--- 查询 {}: 【L10】写入后管道操作 — SET hp=55, SORT hp, REVERSE, TAKE(2) ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.set(&[0, 0, PLAYER_HP], 55)
            .from_players()
            .sort(PLAYER_HP, true)
            .reverse()
            .take(2)
            .build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    // ---- 等级 11: 交叉字段查询 ----
    test_num += 1;
    println!("--- 查询 {}: 【L11】FILTER(hp>30) + SORT(gold) + ANY(level>8) ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players()
            .filter(PLAYER_HP, CMP_GT, 30)
            .sort(PLAYER_GOLD, true)
            .any(PLAYER_LEVEL, CMP_GT, 8)
            .build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    test_num += 1;
    println!("--- 查询 {}: 【L11】FILTER(gold>200) + FILTER(hp>50) + SORT(level) ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players()
            .filter(PLAYER_GOLD, CMP_GT, 200)
            .filter(PLAYER_HP, CMP_GT, 50)
            .sort(PLAYER_LEVEL, true)
            .build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    test_num += 1;
    println!("--- 查询 {}: 【L11】FILTER(gold>200) + SORT(hp) + FIND(level=12) ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players()
            .filter(PLAYER_GOLD, CMP_GT, 200)
            .sort(PLAYER_HP, true)
            .find(PLAYER_LEVEL, CMP_EQ, 12)
            .build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    test_num += 1;
    println!("--- 查询 {}: 【L11】FILTER(hp>50) + SORT(level) + REVERSE + ANY(gold>300) ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players()
            .filter(PLAYER_HP, CMP_GT, 50)
            .sort(PLAYER_LEVEL, true)
            .reverse()
            .any(PLAYER_GOLD, CMP_GT, 300)
            .build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    test_num += 1;
    println!("--- 查询 {}: 【L11】全字段三条件 — hp>25 AND level>4 AND gold>150 ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players()
            .filter(PLAYER_HP, CMP_GT, 25)
            .filter(PLAYER_LEVEL, CMP_GT, 4)
            .filter(PLAYER_GOLD, CMP_GT, 150)
            .build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    // ---- 等级 12: 极限组合压力 ----
    test_num += 1;
    println!("--- 查询 {}: 【L12】15步极限链 — SORT→REVERSE→DROP→TAKE→FILTER→SORT→REVERSE→TAKE→FILTER→SORT→REVERSE→DROP→TAKE→COUNT ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players()
            .sort(PLAYER_LEVEL, true)
            .reverse()
            .drop(1)
            .take(4)
            .filter(PLAYER_HP, CMP_GT, 20)
            .sort(PLAYER_HP, false)
            .reverse()
            .take(3)
            .filter(PLAYER_GOLD, CMP_GT, 100)
            .sort(PLAYER_GOLD, true)
            .reverse()
            .drop(1)
            .take(2)
            .count()
            .build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    test_num += 1;
    println!("--- 查询 {}: 【L12】写入全部 5 个玩家, 然后复杂链查询 ---", test_num);
    println!("    (SET 5个玩家hp → fromPlayers → FILTER hp>30 → SORT hp → REVERSE → TAKE(3))");
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.set(&[0, 0, PLAYER_HP], 100)
            .set(&[0, 1, PLAYER_HP], 200)
            .set(&[0, 2, PLAYER_HP], 150)
            .set(&[0, 3, PLAYER_HP], 50)
            .set(&[0, 4, PLAYER_HP], 175)
            .from_players()
            .filter(PLAYER_HP, CMP_GT, 30)
            .sort(PLAYER_HP, true)
            .reverse()
            .take(3)
            .build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    test_num += 1;
    println!("--- 查询 {}: 【L12】SORT+REVERSE 循环 3 次 — 稳定性测试 ---", test_num);
    println!("    (SORT(level,asc)→REVERSE→SORT(hp,desc)→REVERSE→SORT(gold,asc)→REVERSE)");
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players()
            .sort(PLAYER_LEVEL, true)
            .reverse()
            .sort(PLAYER_HP, false)
            .reverse()
            .sort(PLAYER_GOLD, true)
            .reverse()
            .build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    test_num += 1;
    println!("--- 查询 {}: 【L12】过滤到单元素 + 全操作 — FILTER(hp>85)→SORT→REVERSE→DROP(0)→TAKE(1) ---", test_num);
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.from_players()
            .filter(PLAYER_HP, CMP_GT, 85)
            .sort(PLAYER_LEVEL, true)
            .reverse()
            .drop(0)
            .take(1)
            .build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

    test_num += 1;
    println!("--- 查询 {}: 【L12】极限混合 — 写 + 读 + 排序 + 反转 + 过滤 + 计数 ---", test_num);
    println!("    (SET hp=60→ADD gold=50→fromPlayers→FILTER hp>30→SORT level→REVERSE→DROP(1)→TAKE(3)→FILTER gold>100→SORT hp→REVERSE→TAKE(2)→COUNT)");
    {
        let mut q = SpoiQueryBuilder::new();
        let query = q.set(&[0, 0, PLAYER_HP], 60)
            .add(&[0, 0, PLAYER_GOLD], 50)
            .from_players()
            .filter(PLAYER_HP, CMP_GT, 30)
            .sort(PLAYER_LEVEL, true)
            .reverse()
            .drop(1)
            .take(3)
            .filter(PLAYER_GOLD, CMP_GT, 100)
            .sort(PLAYER_HP, false)
            .reverse()
            .take(2)
            .count()
            .build();
        send_with_length(&mut stream, &query)?;
        print_result(&recv_with_length(&mut stream)?);
        println!();
    }

'@
$newContent = $content.Replace($marker, $insert + $marker)
[System.IO.File]::WriteAllText($path, $newContent)
Write-Output "Done"