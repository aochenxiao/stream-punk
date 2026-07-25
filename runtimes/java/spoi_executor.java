/**
 * StreamPunk SPOI Executor — Java Runtime（v2: 访问器驱动，零反射）
 *
 * SPOI = StreamPunk Operation Instruction
 * 执行 SPOI 指令流，对 Java 对象进行查询/更新操作。
 *
 * 与 v1 的区别：
 *   - 使用 SpoiAccessor 接口替代 java.lang.reflect.Field
 *   - 使用 DeserializeValue（基于 type_id 前缀）替代字节长度启发式
 *   - 导航和字段设置通过访问器的 switch 跳转表，O(1) 且无反射开销
 */

import java.io.ByteArrayOutputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.util.*;

// =============================== 操作码常量 ===============================

class Op {
    // 导航
    static final int NAV        = 0x00;
    static final int IDX        = 0x01;
    static final int DEREF      = 0x02;
    static final int UNWRAP     = 0x03;
    // 写操作
    static final int SET        = 0x04;
    static final int ADD        = 0x05;
    static final int APPEND     = 0x06;
    static final int REMOVE     = 0x07;
    static final int INSERT     = 0x08;
    static final int REPLACE    = 0x09;
    static final int RESET      = 0x0A;
    static final int SETNULL    = 0x0B;
    // 读操作
    static final int FILTER     = 0x0C;
    static final int SELECT     = 0x0D;
    static final int SORT       = 0x0E;
    static final int REVERSE    = 0x0F;
    static final int TAKE       = 0x10;
    static final int DROP       = 0x11;
    static final int TAKEWHILE  = 0x12;
    static final int DROPWHILE  = 0x13;
    static final int DISTINCT   = 0x14;
    // 聚合
    static final int COUNT      = 0x15;
    static final int ANY        = 0x16;
    static final int ALL        = 0x17;
    static final int FIND       = 0x18;
    // 容器
    static final int KEYS       = 0x19;
    static final int VALUES     = 0x1A;
    static final int JOIN       = 0x1B;
    // 控制
    static final int EXEC       = 0x21;
    static final int PIPE       = 0x22;
}

// 路径特殊标记
class PathMarker {
    static final int PATH_DEREF  = 0xFFFF;
    static final int PATH_MAPKEY = 0xFFFE;
}

// 结果类型
class ResultType {
    static final int UNDEF    = 0;
    static final int SINGLE   = 1;
    static final int VECTOR   = 2;
    static final int COUNT    = 3;
    static final int BOOL     = 4;
    static final int OPTIONAL = 5;
    static final int ERROR    = 6;
}

// =============================== SPOI 指令 ===============================

class SpoiInstruction {
    int op;
    ArrayList<Integer> path;
    byte[] operand;

    SpoiInstruction() {
        this.op = 0;
        this.path = new ArrayList<>();
        this.operand = new byte[0];
    }

    SpoiInstruction(int op, ArrayList<Integer> path, byte[] operand) {
        this.op = op;
        this.path = path;
        this.operand = operand;
    }

    @Override
    public String toString() {
        return String.format("SpoiInstruction(op=0x%02X, path=%s, operand_len=%d)",
            op, path, operand.length);
    }
}

// =============================== Varint 编解码 ===============================

class Varint {
    static int readVarint(byte[] data, int[] offset) {
        int result = 0;
        int shift = 0;
        int off = offset[0];
        while (off < data.length) {
            int b = data[off] & 0xFF;
            off++;
            result |= (b & 0x7F) << shift;
            if ((b & 0x80) == 0) {
                offset[0] = off;
                return result;
            }
            shift += 7;
        }
        offset[0] = off;
        return result;
    }

    static void writeVarint(ByteArrayOutputStream buf, int v) {
        while (v >= 0x80) {
            buf.write((v & 0x7F) | 0x80);
            v >>>= 7;
        }
        buf.write(v & 0x7F);
    }
}

// =============================== SPOI 指令解析 ===============================

class SpoiParser {
    static ArrayList<SpoiInstruction> parseSpoiStream(byte[] data) {
        int[] offset = new int[]{0};
        int count = Varint.readVarint(data, offset);
        ArrayList<SpoiInstruction> instructions = new ArrayList<>();

        for (int i = 0; i < count; i++) {
            int op = data[offset[0]] & 0xFF;
            offset[0]++;

            int pathLen = Varint.readVarint(data, offset);
            ArrayList<Integer> path = new ArrayList<>();
            for (int j = 0; j < pathLen; j++) {
                path.add(Varint.readVarint(data, offset));
            }

            int operandLen = Varint.readVarint(data, offset);
            byte[] operand = new byte[operandLen];
            System.arraycopy(data, offset[0], operand, 0, operandLen);
            offset[0] += operandLen;

            instructions.add(new SpoiInstruction(op, path, operand));
        }

        return instructions;
    }
}

// =============================== SPOI 执行器（v2: 访问器驱动） ===============================

class SpoiExecutor {
    Map<String, SpoiAccessor> accessors;
    ArrayList<Object> pipeData;

    public SpoiExecutor(Map<String, SpoiAccessor> accessors) {
        this.accessors = accessors;
        this.pipeData = new ArrayList<>();
    }

    public Map<String, Object> execute(Object root, byte[] data) {
        ArrayList<SpoiInstruction> instructions = SpoiParser.parseSpoiStream(data);
        pipeData = new ArrayList<>();

        for (SpoiInstruction inst : instructions) {
            dispatch(inst, root);
        }

        return makeResult();
    }

    private Map<String, Object> makeResult() {
        Map<String, Object> result = new LinkedHashMap<>();
        if (pipeData.isEmpty()) {
            result.put("resultType", ResultType.UNDEF);
            result.put("data", new byte[0]);
            return result;
        }

        if (pipeData.size() == 1) {
            result.put("resultType", ResultType.SINGLE);
            result.put("value", pipeData.get(0));
        } else {
            result.put("resultType", ResultType.VECTOR);
            result.put("value", pipeData);
        }
        return result;
    }

    // =============================== 调度 ===============================

    private void dispatch(SpoiInstruction inst, Object root) {
        int op = inst.op;

        if (op == Op.SET) {
            opSet(root, inst.path, inst.operand);
        } else if (op == Op.ADD) {
            opAdd(root, inst.path, inst.operand);
        } else if (op == Op.APPEND) {
            opAppend(root, inst.path, inst.operand);
        } else if (op == Op.REMOVE) {
            opRemove(root, inst.path, inst.operand);
        } else if (op == Op.INSERT) {
            opInsert(root, inst.path, inst.operand);
        } else if (op == Op.REPLACE) {
            opReplace(root, inst.path, inst.operand);
        } else if (op == Op.RESET) {
            opReset(root, inst.path);
        } else if (op == Op.SETNULL) {
            opSetnull(root, inst.path);
        } else if (op == Op.FILTER) {
            opFilter(root, inst.path, inst.operand);
        } else if (op == Op.SELECT) {
            opSelect(root, inst.path);
        } else if (op == Op.SORT) {
            opSort(inst.path);
        } else if (op == Op.REVERSE) {
            opReverse();
        } else if (op == Op.TAKE) {
            opTake(inst.operand);
        } else if (op == Op.DROP) {
            opDrop(inst.operand);
        } else if (op == Op.TAKEWHILE) {
            opTakewhile(root, inst.path, inst.operand);
        } else if (op == Op.DROPWHILE) {
            opDropwhile(root, inst.path, inst.operand);
        } else if (op == Op.DISTINCT) {
            opDistinct();
        } else if (op == Op.COUNT) {
            opCount();
        } else if (op == Op.ANY) {
            opAny(root, inst.path, inst.operand);
        } else if (op == Op.ALL) {
            opAll(root, inst.path, inst.operand);
        } else if (op == Op.FIND) {
            opFind(root, inst.path, inst.operand);
        } else if (op == Op.KEYS) {
            opKeys();
        } else if (op == Op.VALUES) {
            opValues();
        } else if (op == Op.JOIN) {
            opJoin();
        } else if (op == Op.EXEC) {
            // 执行结束，结果已在 pipeData 中
        } else if (op == Op.PIPE) {
            opPipe(root, inst.path);
        } else {
            throw new IllegalArgumentException(
                String.format("Unknown SPOI opcode: 0x%02X", op));
        }
    }

    // =============================== 导航（访问器驱动） ===============================

    public Object navigate(Object obj, List<Integer> path) {
        Object current = obj;
        for (int seg : path) {
            current = navStep(current, seg);
        }
        return current;
    }

    Object navStep(Object obj, int seg) {
        // null 处理
        if (obj == null) {
            throw new RuntimeException("Cannot navigate on null");
        }

        // 指针解引用
        if (seg == PathMarker.PATH_DEREF) {
            // 基本类型返回自身
            if (obj instanceof String || obj instanceof Number || obj instanceof Boolean) {
                return obj;
            }
            SpoiAccessor acc = getAccessor(obj);
            if (acc != null) {
                return acc.getField(obj, 0);
            }
            return obj;
        }

        // 容器索引访问
        if (obj instanceof List) {
            return ((List<?>) obj).get(seg);
        }
        if (obj instanceof Map) {
            return new ArrayList<>(((Map<?, ?>) obj).values()).get(seg);
        }

        // 基本类型：seg == 0 时返回自身
        if (obj instanceof String || obj instanceof Number || obj instanceof Boolean) {
            if (seg == 0) {
                return obj;
            }
            throw new RuntimeException(
                "Cannot navigate segment " + seg + " on " + obj.getClass().getSimpleName());
        }

        // 结构体成员访问 — 使用访问器
        SpoiAccessor acc = getAccessor(obj);
        if (acc != null) {
            return acc.getField(obj, seg);
        }

        throw new RuntimeException(
            "Cannot navigate segment " + seg + " on " + obj.getClass().getSimpleName());
    }

    private void navSet(Object obj, List<Integer> path, Object value) {
        if (path.isEmpty()) {
            return;
        }
        if (path.size() == 1) {
            setField(obj, path.get(0), value);
            return;
        }

        Object target = obj;
        for (int i = 0; i < path.size() - 1; i++) {
            target = navStep(target, path.get(i));
        }
        setField(target, path.get(path.size() - 1), value);
    }

    void setField(Object obj, int seg, Object value) {
        if (obj instanceof List) {
            ((List<Object>) obj).set(seg, value);
            return;
        }
        if (obj instanceof Map) {
            Map<Object, Object> map = (Map<Object, Object>) obj;
            List<Object> keys = new ArrayList<>(map.keySet());
            map.put(keys.get(seg), value);
            return;
        }

        // 结构体 — 使用访问器
        SpoiAccessor acc = getAccessor(obj);
        if (acc != null) {
            acc.setField(obj, seg, value);
            return;
        }

        throw new RuntimeException(
            "Cannot set field " + seg + " on " + obj.getClass().getSimpleName());
    }

    /** 获取对象对应的访问器 */
    private SpoiAccessor getAccessor(Object obj) {
        if (obj == null || accessors == null) {
            return null;
        }
        String typeName = obj.getClass().getSimpleName();
        return accessors.get(typeName);
    }

    // =============================== 写操作 ===============================

    private void opSet(Object root, List<Integer> path, byte[] operand) {
        Object value = SpoiDeserializer.deserializeValue(operand);
        navSet(root, path, value);
    }

    private void opAdd(Object root, List<Integer> path, byte[] operand) {
        Object delta = SpoiDeserializer.deserializeValue(operand);
        Object target = navigate(root, path);
        Object result = addValues(target, delta);
        navSet(root, path, result);
    }

    private Object addValues(Object a, Object b) {
        if (a instanceof Number && b instanceof Number) {
            double da = ((Number) a).doubleValue();
            double db = ((Number) b).doubleValue();
            double sum = da + db;
            if (a instanceof Integer || a instanceof Short || a instanceof Byte) {
                if (b instanceof Integer || b instanceof Short || b instanceof Byte) {
                    return (int) sum;
                }
            }
            if (a instanceof Long) {
                return (long) sum;
            }
            if (a instanceof Float) {
                return (float) sum;
            }
            return sum;
        }
        if (a instanceof String || b instanceof String) {
            return String.valueOf(a) + String.valueOf(b);
        }
        throw new RuntimeException(
            "Cannot add " + a.getClass().getSimpleName() + " and " + b.getClass().getSimpleName());
    }

    private void opAppend(Object root, List<Integer> path, byte[] operand) {
        Object value = SpoiDeserializer.deserializeValue(operand);
        Object target = navigate(root, path);
        if (target instanceof List) {
            ((List<Object>) target).add(value);
        } else {
            throw new RuntimeException(
                "Cannot append to " + target.getClass().getSimpleName());
        }
    }

    private void opRemove(Object root, List<Integer> path, byte[] operand) {
        Object target = navigate(root, path);
        if (target instanceof List) {
            int idx = ByteBuffer.wrap(operand).order(ByteOrder.LITTLE_ENDIAN).getInt();
            ((List<?>) target).remove(idx);
        } else {
            throw new RuntimeException(
                "Cannot remove from " + target.getClass().getSimpleName());
        }
    }

    private void opInsert(Object root, List<Integer> path, byte[] operand) {
        int idx = ByteBuffer.wrap(operand, 0, 4).order(ByteOrder.LITTLE_ENDIAN).getInt();
        Object value = SpoiDeserializer.deserializeValue(Arrays.copyOfRange(operand, 4, operand.length));
        Object target = navigate(root, path);
        if (target instanceof List) {
            ((List<Object>) target).add(idx, value);
        } else {
            throw new RuntimeException(
                "Cannot insert into " + target.getClass().getSimpleName());
        }
    }

    private void opReplace(Object root, List<Integer> path, byte[] operand) {
        int idx = ByteBuffer.wrap(operand, 0, 4).order(ByteOrder.LITTLE_ENDIAN).getInt();
        Object value = SpoiDeserializer.deserializeValue(Arrays.copyOfRange(operand, 4, operand.length));
        Object target = navigate(root, path);
        if (target instanceof List) {
            ((List<Object>) target).set(idx, value);
        } else {
            throw new RuntimeException(
                "Cannot replace in " + target.getClass().getSimpleName());
        }
    }

    private void opReset(Object root, List<Integer> path) {
        navSet(root, path, null);
    }

    private void opSetnull(Object root, List<Integer> path) {
        navSet(root, path, null);
    }

    // =============================== 读操作 ===============================

    private void opPipe(Object root, List<Integer> path) {
        Object data;
        if (!path.isEmpty()) {
            data = navigate(root, path);
        } else {
            data = root;
        }

        if (data instanceof List) {
            pipeData = new ArrayList<>((List<?>) data);
        } else if (data instanceof Map) {
            pipeData = new ArrayList<>(((Map<?, ?>) data).values());
        } else {
            pipeData = new ArrayList<>();
            pipeData.add(data);
        }
    }

    /** 检查对象是否匹配比较表达式（v2: 访问器驱动） */
    private boolean matches(Object obj, List<Integer> path, byte[] operand) {
        // operand 格式: memberIdx(u32) + cmpOp(u8) + value_len(varint) + [type_id(u32) + value_bytes]
        if (operand.length < 9) {
            return true;
        }
        int memberIdx = ByteBuffer.wrap(operand, 0, 4).order(ByteOrder.LITTLE_ENDIAN).getInt();
        int cmpOp = operand[4] & 0xFF;
        // 跳过 value_len（varint 编码）
        int[] valueOffset = new int[]{5};
        Varint.readVarint(operand, valueOffset);
        byte[] valueBytes = Arrays.copyOfRange(operand, valueOffset[0], operand.length);

        // 先按路径导航到目标对象，再访问成员字段
        Object target = obj;
        if (!path.isEmpty()) {
            target = navigate(obj, path);
        }
        // 对于基本类型，memberIdx=0 时直接比较值本身
        Object fieldValue;
        if (memberIdx == 0 && (target instanceof Number || target instanceof String || target instanceof Boolean)) {
            fieldValue = target;
        } else {
            fieldValue = navStep(target, memberIdx);
        }
        Object expected = SpoiDeserializer.deserializeValue(valueBytes);

        return compareValues(fieldValue, cmpOp, expected);
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    private boolean compareValues(Object fieldValue, int cmpOp, Object expected) {
        if (fieldValue == null && expected == null) {
            return cmpOp == 0; // eq
        }
        if (fieldValue == null || expected == null) {
            return cmpOp == 1; // ne
        }

        int cmp;
        if (fieldValue instanceof Comparable && expected instanceof Comparable) {
            try {
                cmp = ((Comparable) fieldValue).compareTo(expected);
            } catch (Exception e) {
                cmp = fieldValue.toString().compareTo(expected.toString());
            }
        } else {
            cmp = fieldValue.toString().compareTo(expected.toString());
        }

        switch (cmpOp) {
            case 0: return cmp == 0;  // eq
            case 1: return cmp != 0;  // ne
            case 2: return cmp < 0;   // lt
            case 3: return cmp > 0;   // gt
            case 4: return cmp <= 0;  // le
            case 5: return cmp >= 0;  // ge
            default: return true;
        }
    }

    private void opFilter(Object root, List<Integer> path, byte[] operand) {
        ArrayList<Object> filtered = new ArrayList<>();
        for (Object obj : pipeData) {
            if (matches(obj, path, operand)) {
                filtered.add(obj);
            }
        }
        pipeData = filtered;
    }

    private void opSelect(Object root, List<Integer> path) {
        if (!path.isEmpty()) {
            ArrayList<Object> selected = new ArrayList<>();
            for (Object obj : pipeData) {
                selected.add(navigate(obj, path));
            }
            pipeData = selected;
        }
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    private void opSort(List<Integer> path) {
        if (!path.isEmpty()) {
            pipeData.sort((a, b) -> {
                Object va = navigate(a, path);
                Object vb = navigate(b, path);
                if (va instanceof Comparable && vb instanceof Comparable) {
                    try {
                        return ((Comparable) va).compareTo(vb);
                    } catch (Exception e) {
                        return String.valueOf(va).compareTo(String.valueOf(vb));
                    }
                }
                return String.valueOf(va).compareTo(String.valueOf(vb));
            });
        } else {
            pipeData.sort((a, b) -> String.valueOf(a).compareTo(String.valueOf(b)));
        }
    }

    private void opReverse() {
        Collections.reverse(pipeData);
    }

    private void opTake(byte[] operand) {
        long n = 0;
        if (operand.length >= 4) {
            n = Integer.toUnsignedLong(ByteBuffer.wrap(operand, 0, 4).order(ByteOrder.LITTLE_ENDIAN).getInt());
        }
        if (n > pipeData.size()) {
            n = pipeData.size();
        }
        pipeData = new ArrayList<>(pipeData.subList(0, (int) n));
    }

    private void opDrop(byte[] operand) {
        long n = 0;
        if (operand.length >= 4) {
            n = Integer.toUnsignedLong(ByteBuffer.wrap(operand, 0, 4).order(ByteOrder.LITTLE_ENDIAN).getInt());
        }
        if (n >= pipeData.size()) {
            pipeData = new ArrayList<>();
        } else if (n > 0) {
            pipeData = new ArrayList<>(pipeData.subList((int) n, pipeData.size()));
        }
    }

    private void opTakewhile(Object root, List<Integer> path, byte[] operand) {
        ArrayList<Object> result = new ArrayList<>();
        for (Object obj : pipeData) {
            if (matches(obj, path, operand)) {
                result.add(obj);
            } else {
                break;
            }
        }
        pipeData = result;
    }

    private void opDropwhile(Object root, List<Integer> path, byte[] operand) {
        int idx = pipeData.size();
        for (int i = 0; i < pipeData.size(); i++) {
            if (!matches(pipeData.get(i), path, operand)) {
                idx = i;
                break;
            }
        }
        pipeData = new ArrayList<>(pipeData.subList(idx, pipeData.size()));
    }

    private void opDistinct() {
        LinkedHashSet<Object> seen = new LinkedHashSet<>();
        ArrayList<Object> result = new ArrayList<>();
        for (Object obj : pipeData) {
            Object key = obj;
            if (!(obj instanceof Number) && !(obj instanceof String) && !(obj instanceof Boolean)) {
                key = String.valueOf(obj);
            }
            if (!seen.contains(key)) {
                seen.add(key);
                result.add(obj);
            }
        }
        pipeData = result;
    }

    // =============================== 聚合 ===============================

    private void opCount() {
        int count = pipeData.size();
        pipeData = new ArrayList<>();
        pipeData.add(count);
    }

    private void opAny(Object root, List<Integer> path, byte[] operand) {
        boolean any = false;
        for (Object obj : pipeData) {
            if (matches(obj, path, operand)) {
                any = true;
                break;
            }
        }
        pipeData = new ArrayList<>();
        pipeData.add(any);
    }

    private void opAll(Object root, List<Integer> path, byte[] operand) {
        boolean all = true;
        for (Object obj : pipeData) {
            if (!matches(obj, path, operand)) {
                all = false;
                break;
            }
        }
        pipeData = new ArrayList<>();
        pipeData.add(all);
    }

    private void opFind(Object root, List<Integer> path, byte[] operand) {
        for (Object obj : pipeData) {
            if (matches(obj, path, operand)) {
                pipeData = new ArrayList<>();
                pipeData.add(obj);
                return;
            }
        }
        pipeData = new ArrayList<>();
    }

    // =============================== 容器操作 ===============================

    private void opKeys() {
        ArrayList<Object> result = new ArrayList<>();
        for (Object obj : pipeData) {
            if (obj instanceof Map) {
                result.addAll(((Map<?, ?>) obj).keySet());
            }
        }
        pipeData = result;
    }

    private void opValues() {
        ArrayList<Object> result = new ArrayList<>();
        for (Object obj : pipeData) {
            if (obj instanceof Map) {
                result.addAll(((Map<?, ?>) obj).values());
            }
        }
        pipeData = result;
    }

    private void opJoin() {
        ArrayList<Object> result = new ArrayList<>();
        for (Object obj : pipeData) {
            if (obj instanceof List) {
                result.addAll((List<?>) obj);
            } else {
                result.add(obj);
            }
        }
        pipeData = result;
    }
}