#ifndef VM_HPP
#define VM_HPP

#include <vector>
#include <string>
#include <iostream>
#include <cmath>
#include "lexer.hpp"
#include "symbolTable.hpp"
#include "builtinFunc.hpp"

namespace tool::vm
{
    using namespace tool::lex;
    using namespace tool::sym;
    using namespace tool::builtin;

    struct Env
    {
        std::unordered_map<std::string, tool::lex::Value> table;
        Env *parent = nullptr;

        std::optional<tool::lex::Value> lookup(const std::string &name) const
        {
            auto it = table.find(name);
            if (it != table.end())
                return it->second;
            if (parent)
                return parent->lookup(name);
            return std::nullopt;
        }
    };

    struct Frame
    {
        std::string funcName;
        RpnList rpn;
        int pc;
        std::vector<Value> stack;
        std::unordered_map<std::string, Value> localEnv;
        bool isConstructor = false; // 构造函数返回时把 this 对象带回调用方
    };

    // 生成基础类型的默认值（数组默认元素、越界读用）
    inline Value defaultValue(Value::Type t)
    {
        Value v{};
        v.type = t;
        switch (t)
        {
        case Value::INT:
            v.intVal = 0;
            break;
        case Value::BOOL:
            v.boolVal = false;
            break;
        case Value::STRING:
            v.strVal = "";
            break;
        case Value::CHAR:
            v.charVal = 0;
            v.intVal = 0;
            break;
        case Value::FLOAT:
            v.floatVal = 0.0f;
            break;
        default:
            break;
        }
        return v;
    }

    // 递归解析引用变量（放在类前面，确保类内能找到）
    // 找不到返回 nullptr，由调用方决定怎么报错（不要在这里清空调用栈）
    inline Value *findVarRef(std::vector<Frame> &callStack, size_t frameIdx, const std::string &name)
    {
        if (frameIdx >= callStack.size())
            return nullptr;
        Frame &f = callStack[frameIdx];
        auto it = f.localEnv.find(name);
        if (it != f.localEnv.end())
        {
            if (it->second.isRef)
            {
                // 优先使用描述符里记录的绝对帧号（this、方法调用会跨帧）
                if (it->second.refFrame >= 0 && it->second.refFrame < (int)callStack.size())
                    return findVarRef(callStack, (size_t)it->second.refFrame, it->second.refTarget);
                if (frameIdx == 0)
                    return nullptr;
                return findVarRef(callStack, frameIdx - 1, it->second.refTarget);
            }
            return &it->second;
        }

        // 当前帧找不到，并且当前不是main帧(frameIdx!=0)，去0号帧(全局main帧)查找全局变量
        if (frameIdx != 0)
        {
            return findVarRef(callStack, 0, name);
        }
        return nullptr;
    }

    // 解析一个“可能是引用”的值；非引用直接返回自身地址
    inline Value *resolveRefValue(std::vector<Frame> &callStack, Value &v)
    {
        if (!v.isRef)
            return &v;

        size_t frameIdx = (v.refFrame >= 0) ? (size_t)v.refFrame : (callStack.size() - 1);
        return findVarRef(callStack, frameIdx, v.refTarget);
    }

    // 赋值：保留目标变量原有的类型/const 标记，并处理 char、int、float 的隐式转换
    inline void assignPreserveType(Value &dst, const Value &src)
    {
        Value::Type oldType = dst.type;
        bool oldConst = dst.isConst;
        bool oldRef = dst.isRef;
        std::string oldRefTarget = dst.refTarget;
        int oldRefFrame = dst.refFrame;

        if (src.type == Value::VD)
        {
            dst = defaultValue(oldType);
        }
        else
        {
            dst = src;
        }
        dst.type = oldType;
        dst.isConst = oldConst;
        dst.isRef = oldRef;
        dst.refTarget = oldRefTarget;
        dst.refFrame = oldRefFrame;

        if (oldType == Value::CHAR && src.type == Value::INT)
        {
            // int → char：按 ASCII 取低 8 位
            dst.charVal = (char)(src.intVal & 0xFF);
            dst.intVal = src.intVal & 0xFF;
        }
        else if (oldType == Value::INT && src.type == Value::CHAR)
        {
            // char → int：直接使用 ASCII 码
            dst.intVal = src.intVal;
        }
        else if (oldType == Value::FLOAT && src.type == Value::INT)
        {
            dst.floatVal = (float)src.intVal;
        }
        else if (oldType == Value::INT && src.type == Value::FLOAT)
        {
            dst.intVal = (int)src.floatVal;
        }
    }
    // 递归构造多维数组：dims[level..] 是剩余维度，leaf 是最内层元素类型
    inline Value makeArray(const std::vector<int> &dims, size_t level, Value::Type leaf)
    {
        if (level >= dims.size())
            return defaultValue(leaf);

        Value v{};
        v.type = Value::ARRAY;
        v.elemType = leaf;
        v.arrayDepth = (int)(dims.size() - level);
        v.arrayVal.reserve(dims[level] > 0 ? dims[level] : 0);
        for (int i = 0; i < dims[level]; i++)
            v.arrayVal.push_back(makeArray(dims, level + 1, leaf));
        return v;
    }

    class VM
    {
    public:
        std::vector<Frame> callStack;

        Value execute(const RpnList &mainRpn)
        {
            Frame mainFrame;
            mainFrame.funcName = "<main>";
            mainFrame.rpn = mainRpn;
            mainFrame.pc = 0;
            callStack.push_back(std::move(mainFrame));

            while (!callStack.empty())
            {
                Frame &fr = callStack.back();
                if (fr.pc >= (int)fr.rpn.size())
                {
                    // 构造函数跑到末尾（没有显式 return）时，也要把 this 对象带回调用方
                    if (fr.isConstructor)
                    {
                        Value obj{};
                        auto it = fr.localEnv.find("this");
                        if (it != fr.localEnv.end())
                            obj = it->second;
                        callStack.pop_back();
                        if (!callStack.empty())
                            callStack.back().stack.push_back(std::move(obj));
                    }
                    else
                    {
                        callStack.pop_back();
                    }
                    continue;
                }

                const RpnOp &op = fr.rpn[fr.pc];
                fr.pc++;

                switch (op.op)
                {
                case OP_PUSH_INT:
                {
                    Value v;
                    v.type = Value::INT;
                    v.intVal = op.intArg;
                    fr.stack.push_back(v);
                    break;
                }
                case OP_PUSH_BOOL:
                {
                    Value v;
                    v.type = Value::BOOL;
                    v.boolVal = op.boolArg;
                    fr.stack.push_back(v);
                    break;
                }
                case OP_PUSH_STR:
                {
                    Value v;
                    v.type = Value::STRING;
                    v.strVal = op.strArg;
                    fr.stack.push_back(v);
                    break;
                }
                case OP_PUSH_CHAR:
                {
                    Value v;
                    v.type = Value::CHAR;
                    v.charVal = op.charVal;
                    v.intVal = (unsigned char)op.charVal; // 同步 ASCII 码
                    fr.stack.push_back(v);
                    break;
                }

                case OP_PUSH_REF:
                {
                    Value v{};
                    v.isRef = true;
                    v.refTarget = op.strArg;
                    v.refFrame = (int)callStack.size() - 1;
                    v.type = Value::INT;
                    fr.stack.push_back(v);
                    break;
                }
                case OP_POP:
                {
                    if (!fr.stack.empty())
                        fr.stack.pop_back();
                    break;
                }
                case OP_LOAD_VAR:
                {
                    size_t curIdx = callStack.size() - 1;
                    Value *v = findVarRef(callStack, curIdx, op.strArg);
                    if (!v)
                    {
                        std::cerr << "error: undefined variable '" << op.strArg << "'" << std::endl;
                        callStack.clear();
                        return Value{};
                    }
                    fr.stack.push_back(*v);
                    break;
                }
                case OP_STORE_VAR:
                {
                    std::string varName = op.strArg;
                    Value val = fr.stack.back();
                    fr.stack.pop_back();

                    // 存进普通变量前先解析引用（例如 var P q = this;）
                    if (val.isRef)
                    {
                        Value *r = resolveRefValue(callStack, val);
                        if (r)
                            val = *r;
                    }

                    size_t curIdx = callStack.size() - 1;
                    Frame &f = callStack[curIdx];
                    auto it = f.localEnv.find(varName);

                    if (it == f.localEnv.end())
                    {
                        // 变量不存在：首次赋值，直接创建
                        f.localEnv[varName] = val;
                        break;
                    }

                    // 定位真正要写入的位置：普通变量就是自己，ref 变量则穿透到调用方
                    Value *slot = findVarRef(callStack, curIdx, varName);
                    if (!slot)
                    {
                        std::cerr << "error: undefined ref target for '" << varName << "'" << std::endl;
                        callStack.clear();
                        return Value{};
                    }
                    if (slot->isConst)
                    {
                        std::cerr << "error: cannot modify const variable '" << varName << "'" << std::endl;
                        callStack.clear();
                        return Value{};
                    }
                    assignPreserveType(*slot, val);
                    break;
                }

                case OP_ADD:
                case OP_SUB:
                case OP_MUL:
                case OP_DIV:
                case OP_MOD:
                {
                    Value b = fr.stack.back();
                    fr.stack.pop_back();
                    Value a = fr.stack.back();
                    fr.stack.pop_back();
                    Value res;
                    if (a.type == Value::VD || b.type == Value::VD)
                    {
                        std::cerr << "error: vd cannot be used in calculation" << std::endl;
                        callStack.clear();
                        return Value{};
                    }
                    if (op.op == OP_ADD && (a.type == Value::STRING || b.type == Value::STRING))
                    {
                        // 字符串拼接逻辑（保持不变）
                        res.type = Value::STRING;
                        std::string left, right;
                        if (a.type == Value::STRING)
                            left = a.strVal;
                        else if (a.type == Value::CHAR)
                            left = std::string(1, a.charVal);
                        if (b.type == Value::STRING)
                            right = b.strVal;
                        else if (b.type == Value::CHAR)
                            right = std::string(1, b.charVal);
                        res.strVal = left + right;
                    }
                    else if (a.type == Value::FLOAT || b.type == Value::FLOAT)
                    {
                        // 浮点运算：int 自动转 float
                        res.type = Value::FLOAT;
                        float af = (a.type == Value::FLOAT) ? a.floatVal : (float)a.intVal;
                        float bf = (b.type == Value::FLOAT) ? b.floatVal : (float)b.intVal;

                        if (op.op == OP_ADD)
                            res.floatVal = af + bf;
                        else if (op.op == OP_SUB)
                            res.floatVal = af - bf;
                        else if (op.op == OP_MUL)
                            res.floatVal = af * bf;
                        else if (op.op == OP_DIV)
                            res.floatVal = af / bf;
                        else if (op.op == OP_MOD)
                            res.floatVal = fmod(af, bf);
                    }
                    else
                    {
                        // 整数运算逻辑（保持不变）
                        res.type = Value::INT;
                        if (op.op == OP_ADD)
                            res.intVal = a.intVal + b.intVal;
                        else if (op.op == OP_SUB)
                            res.intVal = a.intVal - b.intVal;
                        else if (op.op == OP_MUL)
                            res.intVal = a.intVal * b.intVal;
                        else if (op.op == OP_DIV)
                            res.intVal = a.intVal / b.intVal;
                        else if (op.op == OP_MOD)
                            res.intVal = a.intVal % b.intVal;
                    }

                    fr.stack.push_back(res);
                    break;
                }

                case OP_GT:
                case OP_LT:
                case OP_GE:
                case OP_LE:
                case OP_EQ:
                case OP_NE:
                case OP_APPROX_EQ:
                {
                    Value b = fr.stack.back();
                    fr.stack.pop_back();
                    Value a = fr.stack.back();
                    fr.stack.pop_back();
                    Value res;
                    res.type = Value::BOOL;
                    double av = (a.type == Value::FLOAT) ? a.floatVal : a.intVal;
                    double bv = (b.type == Value::FLOAT) ? b.floatVal : b.intVal;

                    switch (op.op)
                    {
                    case OP_GT:
                        res.boolVal = (av > bv);
                        break;
                    case OP_LT:
                        res.boolVal = (av < bv);
                        break;
                    case OP_GE:
                        res.boolVal = (av >= bv);
                        break;
                    case OP_LE:
                        res.boolVal = (av <= bv);
                        break;
                    case OP_EQ:
                        res.boolVal = (av == bv);
                        break;
                    case OP_NE:
                        res.boolVal = (av != bv);
                        break;
                    case OP_APPROX_EQ:
                        res.boolVal = (fabs(av - bv) <= 0.5);
                        break;
                    default:
                        break;
                    }
                    fr.stack.push_back(res);
                    break;
                }
                case OP_JMP:
                {
                    fr.pc = op.intArg;
                    break;
                }
                case OP_JMP_IF_FALSE:
                {
                    Value cond = fr.stack.back();
                    fr.stack.pop_back();
                    bool isTrue = false;
                    if (cond.type == Value::BOOL)
                        isTrue = cond.boolVal;
                    else if (cond.type == Value::INT)
                        isTrue = (cond.intVal != 0);
                    if (!isTrue)
                        fr.pc = op.intArg;
                    break;
                }
                case OP_CALL:
                {
                    FuncInfo *fi = lookupFunc(op.strArg);
                    // ===== 成员方法调用 =====
                    if (op.boolArg)
                    {
                        std::vector<Value> args;
                        for (int i = 0; i < op.intArg; i++)
                        {
                            args.push_back(fr.stack.back());
                            fr.stack.pop_back();
                        }
                        Value obj = fr.stack.back();
                        fr.stack.pop_back();

                        // obj 可能是对象引用描述符，先解析出真实对象
                        Value *real = resolveRefValue(callStack, obj);
                        if (!real)
                        {
                            std::cerr << "error: undefined object reference '" << obj.refTarget << "'" << std::endl;
                            callStack.clear();
                            return Value{};
                        }

                        if (real->type != Value::OBJECT)
                        {
                            std::cerr << "error: cannot call method on non-object" << std::endl;
                            callStack.clear();
                            return Value{};
                        }

                        ClassInfo *cls = lookupClass(real->objectClass);
                        if (!cls)
                        {
                            std::cerr << "error: undefined class '" << real->objectClass << "'" << std::endl;
                            callStack.clear();
                            return Value{};
                        }
                        auto methodIt = cls->methods.find(op.strArg);
                        if (methodIt == cls->methods.end())
                        {
                            std::cerr << "error: undefined method '" << op.strArg << "'" << std::endl;
                            callStack.clear();
                            return Value{};
                        }
                        fi = &methodIt->second;

                        Frame newFrame;
                        newFrame.funcName = op.strArg;
                        newFrame.rpn = fi->body;
                        newFrame.pc = 0;
                        // this 保留引用描述符：方法里对 this 成员的写会直接落回原对象
                        newFrame.localEnv["this"] = obj;

                        for (size_t i = 0; i < fi->params.size(); i++)
                        {
                            size_t argIdx = args.size() - 1 - i;
                            if (argIdx < args.size())
                            {
                                Value arg = args[argIdx];
                                if (!fi->params[i].isRef && arg.isRef)
                                {
                                    Value *r = resolveRefValue(callStack, arg);
                                    if (r)
                                        arg = *r;
                                }
                                newFrame.localEnv[fi->params[i].name] = arg;
                            }
                        }

                        callStack.push_back(std::move(newFrame));
                        break;
                    }

                    if (!fi)
                    {
                        std::cerr << "error: undefined function '" << op.strArg << "'" << std::endl;
                        callStack.clear();
                        return Value{};
                    }

                    int argCnt = op.intArg;
                    std::vector<Value> args;
                    for (int i = 0; i < argCnt; i++)
                    {
                        args.push_back(fr.stack.back());
                        fr.stack.pop_back();
                    }

                    if (fi->isBuiltin)
                    {
                        Value ret = fi->handler(args);
                        if (fi->retType != "void")
                            fr.stack.push_back(ret);
                        break;
                    }

                    Frame newFrame;
                    newFrame.funcName = op.strArg;
                    newFrame.rpn = fi->body;
                    newFrame.pc = 0;

                    for (size_t i = 0; i < fi->params.size() && i < args.size(); i++)
                    {
                        size_t argIdx = args.size() - 1 - i;
                        Value arg = args[argIdx];
                        if (!fi->params[i].isRef && arg.isRef)
                        {
                            Value *r = resolveRefValue(callStack, arg);
                            if (r)
                                arg = *r;
                        }
                        newFrame.localEnv[fi->params[i].name] = arg;
                    }

                    callStack.push_back(std::move(newFrame));
                    break;
                }
                case OP_CALL_CTOR:
                {
                    int argCnt = op.intArg;
                    std::vector<Value> args;
                    for (int i = 0; i < argCnt; i++)
                    {
                        if (fr.stack.empty())
                        {
                            std::cerr << "error: missing constructor arguments" << std::endl;
                            callStack.clear();
                            return Value{};
                        }
                        args.push_back(fr.stack.back());
                        fr.stack.pop_back();
                    }

                    if (fr.stack.empty())
                    {
                        std::cerr << "error: missing object for constructor" << std::endl;
                        callStack.clear();
                        return Value{};
                    }
                    Value obj = fr.stack.back();
                    fr.stack.pop_back();

                    if (obj.type != Value::OBJECT)
                    {
                        std::cerr << "error: constructor called on non-object" << std::endl;
                        callStack.clear();
                        return Value{};
                    }

                    ClassInfo *cls = lookupClass(obj.objectClass);
                    if (!cls)
                    {
                        std::cerr << "error: undefined class '" << obj.objectClass << "'" << std::endl;
                        callStack.clear();
                        return Value{};
                    }

                    auto mit = cls->methods.find(op.strArg);
                    if (mit == cls->methods.end())
                    {
                        std::cerr << "error: undefined constructor '" << op.strArg << "'" << std::endl;
                        callStack.clear();
                        return Value{};
                    }
                    FuncInfo *ctor = &mit->second;

                    Frame newFrame;
                    newFrame.funcName = op.strArg;
                    newFrame.rpn = ctor->body;
                    newFrame.pc = 0;
                    newFrame.isConstructor = true;
                    newFrame.localEnv["this"] = obj;

                    for (size_t i = 0; i < ctor->params.size() && i < args.size(); i++)
                    {
                        size_t argIdx = args.size() - 1 - i;
                        newFrame.localEnv[ctor->params[i].name] = args[argIdx];
                    }

                    callStack.push_back(std::move(newFrame));
                    break;
                }
                case OP_RET:
                {
                    Value retVal;

                    if (fr.isConstructor)
                    {
                        // 构造函数：返回修改后的 this 对象
                        auto it = fr.localEnv.find("this");
                        if (it != fr.localEnv.end())
                            retVal = it->second;
                    }
                    else
                    {
                        if (!fr.stack.empty())
                            retVal = fr.stack.back();

                        // return this 这类引用要按值返回，避免调用方拿到悬空描述符
                        if (retVal.isRef)
                        {
                            Value *r = resolveRefValue(callStack, retVal);
                            if (r)
                                retVal = *r;
                        }
                    }

                    callStack.pop_back();
                    if (!callStack.empty())
                        callStack.back().stack.push_back(retVal);
                    break;
                }
                case OP_NEW_ARRAY:
                {
                    int size = fr.stack.back().intVal;
                    fr.stack.pop_back();
                    Value arr;
                    arr.type = Value::ARRAY;
                    arr.elemType = (Value::Type)op.intArg;
                    arr.arrayDepth = 1;
                    // 按元素类型填充默认值
                    for (int i = 0; i < size; i++)
                    {
                        Value elem{};
                        elem.type = arr.elemType;
                        switch (arr.elemType)
                        {
                        case Value::INT:
                            elem.intVal = 0;
                            break;
                        case Value::BOOL:
                            elem.boolVal = false;
                            break;
                        case Value::STRING:
                            elem.strVal = "";
                            break;
                        case Value::CHAR:
                            elem.charVal = 0;
                            elem.intVal = 0;
                            break;
                        case Value::FLOAT:
                            elem.floatVal = 0.0f;
                            break;
                        default:
                            break;
                        }
                        arr.arrayVal.push_back(elem);
                    }
                    fr.stack.push_back(arr);
                    break;
                }
                case OP_NEW_ARRAY_ND:
                {
                    int ndims = op.intArg2;
                    if (ndims <= 0)
                    {
                        std::cerr << "error: invalid array dimensions" << std::endl;
                        callStack.clear();
                        return Value{};
                    }
                    if ((int)fr.stack.size() < ndims)
                    {
                        std::cerr << "error: missing array size" << std::endl;
                        callStack.clear();
                        return Value{};
                    }

                    std::vector<int> dims(ndims);
                    for (int i = ndims - 1; i >= 0; --i)
                    {
                        dims[i] = fr.stack.back().intVal;
                        fr.stack.pop_back();
                        if (dims[i] < 0)
                        {
                            std::cerr << "error: array size cannot be negative" << std::endl;
                            callStack.clear();
                            return Value{};
                        }
                    }

                    fr.stack.push_back(makeArray(dims, 0, (Value::Type)op.intArg));
                    break;
                }
                case OP_ARRAY_LOAD:
                {
                    int idx = fr.stack.back().intVal;
                    fr.stack.pop_back();
                    // 栈上的数组是临时副本，直接移动，避免整份深拷贝
                    Value arr = std::move(fr.stack.back());
                    fr.stack.pop_back();
                    Value res{};
                    if (idx >= 0 && idx < (int)arr.arrayVal.size())
                    {
                        res = std::move(arr.arrayVal[idx]);
                    }
                    else if (arr.arrayDepth > 1)
                    {
                        // 多维数组越界：返回同类型的空子数组，后续下标继续按默认值处理
                        res.type = Value::ARRAY;
                        res.elemType = arr.elemType;
                        res.arrayDepth = arr.arrayDepth - 1;
                    }
                    else
                    {
                        // 一维越界默认返回 0/空
                        res = defaultValue(arr.elemType);
                    }
                    fr.stack.push_back(std::move(res));
                    break;
                }
                case OP_ARRAY_STORE:
                {
                    Value val = fr.stack.back();
                    fr.stack.pop_back();
                    int idx = fr.stack.back().intVal;
                    fr.stack.pop_back();
                    Value &arr = fr.stack.back(); // 引用栈上数组，直接修改
                    if (idx < 0 || idx >= (int)arr.arrayVal.size())
                    {
                        std::cerr << "error: array index " << idx << " out of bounds" << std::endl;
                        callStack.clear();
                        return Value{};
                    }
                    // 保留元素类型，只更新值
                    Value::Type oldType = arr.arrayVal[idx].type;
                    arr.arrayVal[idx] = val;
                    arr.arrayVal[idx].type = oldType;
                    // char 与 int 隐式转换
                    if (oldType == Value::CHAR && val.type == Value::INT)
                    {
                        arr.arrayVal[idx].charVal = (char)(val.intVal & 0xFF);
                        arr.arrayVal[idx].intVal = val.intVal & 0xFF;
                    }
                    else if (oldType == Value::INT && val.type == Value::CHAR)
                    {
                        arr.arrayVal[idx].intVal = val.intVal;
                    }
                    // float 隐式转换
                    else if (oldType == Value::FLOAT && val.type == Value::INT)
                    {
                        arr.arrayVal[idx].floatVal = (float)val.intVal;
                    }
                    else if (oldType == Value::INT && val.type == Value::FLOAT)
                    {
                        arr.arrayVal[idx].intVal = (int)val.floatVal;
                    }

                    break;
                }
                case OP_ARRAY_STORE_N:
                {
                    int ndims = op.intArg;
                    if (ndims <= 0 || (int)fr.stack.size() < ndims + 2)
                    {
                        std::cerr << "error: malformed multidimensional assignment" << std::endl;
                        callStack.clear();
                        return Value{};
                    }

                    Value val = fr.stack.back();
                    fr.stack.pop_back();

                    std::vector<int> idxs(ndims);
                    for (int i = ndims - 1; i >= 0; --i)
                    {
                        idxs[i] = fr.stack.back().intVal;
                        fr.stack.pop_back();
                    }

                    Value *slot = &fr.stack.back();
                    for (int level = 0; level < ndims; level++)
                    {
                        if (slot->type != Value::ARRAY)
                        {
                            std::cerr << "error: cannot index non-array value" << std::endl;
                            callStack.clear();
                            return Value{};
                        }
                        int idx = idxs[level];
                        if (idx < 0 || idx >= (int)slot->arrayVal.size())
                        {
                            std::cerr << "error: array index " << idx << " out of bounds" << std::endl;
                            callStack.clear();
                            return Value{};
                        }

                        if (level == ndims - 1)
                            assignPreserveType(slot->arrayVal[idx], val);
                        else
                            slot = &slot->arrayVal[idx];
                    }
                    break;
                }

                // ========== 新增：对象相关指令（一并补齐） ==========
                case OP_NEW_OBJECT:
                {
                    std::string className = op.strArg;
                    ClassInfo *cls = lookupClass(className);
                    if (!cls)
                    {
                        std::cerr << "error: undefined class '" << className << "'" << std::endl;
                        callStack.clear();
                        return Value{};
                    }
                    Value obj;
                    obj.type = Value::OBJECT;
                    obj.objectClass = className;
                    // 按类声明初始化默认值
                    for (auto &[name, memInfo] : cls->members)
                    {
                        Value mem{};

                        if (memInfo.isArray)
                        {
                            std::vector<int> dims{memInfo.arraySize};
                            mem = makeArray(dims, 0, memInfo.elemType);
                            obj.memberNames.push_back(name);
                            obj.memberValues.push_back(std::move(mem));
                            continue;
                        }

                        const std::string &t = memInfo.type;
                        if (t == "int")
                        {
                            mem.type = Value::INT;
                            mem.intVal = 0;
                        }
                        else if (t == "bool")
                        {
                            mem.type = Value::BOOL;
                            mem.boolVal = false;
                        }
                        else if (t == "string")
                        {
                            mem.type = Value::STRING;
                            mem.strVal = "";
                        }
                        else if (t == "char")
                        {
                            mem.type = Value::CHAR;
                            mem.charVal = 0;
                        }
                        else if (t == "float")
                        {
                            mem.type = Value::FLOAT;
                            mem.floatVal = 0.0f;
                        }
                        else
                        {
                            // --------------------------
                            // 【这里！】成员是另一个类/模板实例！
                            // --------------------------
                            ClassInfo *subCls = lookupClass(t);
                            if (subCls == nullptr)
                            {
                                // 找不到类，填充空占位
                                mem.type = Value::V_VOID;
                            }
                            else
                            {
                                // 1.新建子对象
                                mem.type = Value::OBJECT;
                                mem.objectClass = t;
                                // 2.调用子对象无参构造函数
                                std::vector<Value> emptyArgs;
                                // 查找无参构造mangled名
                                std::string ctorName = tool::parse::constructorMangledName(t, {});
                                auto mit = subCls->methods.find(ctorName);
                                if (mit != subCls->methods.end())
                                {
                                    // 调用构造：复用VM内部构造调用逻辑
                                    Frame subFrame;
                                    subFrame.funcName = ctorName;
                                    subFrame.rpn = mit->second.body;
                                    subFrame.pc = 0;
                                    subFrame.isConstructor = true;
                                    subFrame.localEnv["this"] = mem;
                                    callStack.push_back(std::move(subFrame));

                                    // =====问题说明=====
                                    // 当前OP_NEW_OBJECT是单条RPN指令，**不允许手动push Frame到callStack**。
                                    // RPN设计：只能顺序执行op，不能中途插入别的帧执行。
                                    // 所以：OP_NEW_OBJECT指令**不能直接调用子成员的构造函数！**
                                }
                            }
                        }
                        obj.memberNames.push_back(name);
                        obj.memberValues.push_back(mem);
                    }
                    fr.stack.push_back(obj);
                    break;
                }
                case OP_NOT:
                {
                    Value v = fr.stack.back();
                    fr.stack.pop_back();
                    // toBool: 复用你VM内部已有的bool转换逻辑
                    auto toBool = [](const Value &val) -> bool
                    {
                        if (val.type == Value::BOOL)
                            return val.boolVal;
                        if (val.type == Value::INT || val.type == Value::CHAR)
                            return val.intVal != 0;
                        if (val.type == Value::FLOAT)
                            return val.floatVal != 0.0f;
                        return false;
                    };
                    Value res;
                    res.type = Value::BOOL;
                    res.boolVal = !toBool(v);
                    fr.stack.push_back(res);
                    break;
                }
                case OP_JUMP_BREAK:
                {
                    // intArg 已经回填好循环出口RPN下标
                    fr.pc = op.intArg;
                    break;
                }
                case OP_MEMBER_LOAD:
                {
                    std::string memName = op.strArg;
                    Value obj = fr.stack.back();
                    fr.stack.pop_back();

                    Value *real = resolveRefValue(callStack, obj);
                    if (!real)
                    {
                        std::cerr << "error: undefined object reference '" << obj.refTarget << "'" << std::endl;
                        callStack.clear();
                        return Value{};
                    }
                    if (real->type != Value::OBJECT)
                    {
                        std::cerr << "error: cannot access member on non-object" << std::endl;
                        callStack.clear();
                        return Value{};
                    }
                    // 遍历查找成员
                    int idx = -1;
                    for (int i = 0; i < (int)real->memberNames.size(); i++)
                    {
                        if (real->memberNames[i] == memName)
                        {
                            idx = i;
                            break;
                        }
                    }
                    if (idx == -1)
                    {
                        std::cerr << "error: undefined member '" << memName << "'" << std::endl;
                        callStack.clear();
                        return Value{};
                    }
                    fr.stack.push_back(real->memberValues[idx]);
                    break;
                }
                case OP_MEMBER_STORE:
                {
                    std::string memName = op.strArg;
                    Value val = fr.stack.back();
                    fr.stack.pop_back();
                    Value &obj = fr.stack.back();

                    Value *real = resolveRefValue(callStack, obj);
                    if (!real)
                    {
                        std::cerr << "error: undefined object reference '" << obj.refTarget << "'" << std::endl;
                        callStack.clear();
                        return Value{};
                    }
                    if (real->type != Value::OBJECT)
                    {
                        std::cerr << "error: cannot modify member on non-object" << std::endl;
                        callStack.clear();
                        return Value{};
                    }
                    // 遍历查找成员
                    int idx = -1;
                    for (int i = 0; i < (int)real->memberNames.size(); i++)
                    {
                        if (real->memberNames[i] == memName)
                        {
                            idx = i;
                            break;
                        }
                    }
                    if (idx == -1)
                    {
                        std::cerr << "error: undefined member '" << memName << "'" << std::endl;
                        callStack.clear();
                        return Value{};
                    }
                    // 保留成员原类型并处理隐式转换
                    assignPreserveType(real->memberValues[idx], val);
                    break;
                }
                case OP_PUSH_OBJ_REF:
                {
                    Value v{};
                    v.type = Value::OBJECT;
                    v.isRef = true;
                    v.refTarget = op.strArg;
                    v.refFrame = (int)callStack.size() - 1;
                    fr.stack.push_back(std::move(v));
                    break;
                }
                case OP_PUSH_THIS:
                {
                    if (fr.localEnv.find("this") == fr.localEnv.end())
                    {
                        std::cerr << "error: 'this' used outside class method" << std::endl;
                        callStack.clear();
                        return Value{};
                    }
                    // 压入指向本帧 this 槽的引用：
                    // - 普通方法里 localEnv["this"] 是指向调用方对象的引用，会继续穿透；
                    // - 构造函数里 localEnv["this"] 是对象副本，修改会保留到构造结果。
                    Value v{};
                    v.type = Value::OBJECT;
                    v.isRef = true;
                    v.refTarget = "this";
                    v.refFrame = (int)callStack.size() - 1;
                    fr.stack.push_back(std::move(v));
                    break;
                }
                case OP_PUSH_FLOAT:
                {
                    Value v;
                    v.type = Value::FLOAT;
                    v.floatVal = op.floatArg;
                    fr.stack.push_back(v);
                    break;
                }

                case OP_FACT:
                {
                    Value val = fr.stack.back();
                    fr.stack.pop_back();
                    Value res;
                    res.type = Value::INT;

                    // 统一转整数
                    int n;
                    if (val.type == Value::INT || val.type == Value::CHAR)
                        n = val.intVal;
                    else if (val.type == Value::FLOAT)
                        n = (int)val.floatVal;
                    else
                    {
                        std::cerr << "error: fact requires numeric argument" << std::endl;
                        callStack.clear();
                        return Value{};
                    }

                    if (n < 0)
                    {
                        std::cerr << "error: factorial of negative number" << std::endl;
                        callStack.clear();
                        return Value{};
                    }

                    int result = 1;
                    for (int i = 2; i <= n; i++)
                    {
                        result *= i;
                    }
                    res.intVal = result;
                    fr.stack.push_back(res);
                    break;
                }

                case OP_SQRT:
                {
                    Value val = fr.stack.back();
                    fr.stack.pop_back();
                    Value res;
                    res.type = Value::FLOAT;

                    // 类型校验：仅整数
                    if (val.type != Value::INT && val.type != Value::CHAR)
                    {
                        std::cerr << "error: sqrt requires integer argument" << std::endl;
                        callStack.clear();
                        return Value{};
                    }

                    int n = val.intVal;
                    // 正整数校验
                    if (n <= 0)
                    {
                        std::cerr << "error: sqrt requires positive integer" << std::endl;
                        callStack.clear();
                        return Value{};
                    }

                    res.floatVal = sqrt((double)n);
                    fr.stack.push_back(res);
                    break;
                }

                case OP_CAST:
                {
                    Value val = fr.stack.back();
                    fr.stack.pop_back();
                    Value::Type target = (Value::Type)op.intArg;
                    Value res;
                    res.type = target;

                    switch (target)
                    {
                    case Value::INT:
                        if (val.type == Value::FLOAT)
                            res.intVal = (int)val.floatVal;
                        else if (val.type == Value::BOOL)
                            res.intVal = val.boolVal ? 1 : 0;
                        else
                            res.intVal = val.intVal; // int/char 直接用
                        break;

                    case Value::FLOAT:
                        if (val.type == Value::INT || val.type == Value::CHAR)
                            res.floatVal = (float)val.intVal;
                        else if (val.type == Value::BOOL)
                            res.floatVal = val.boolVal ? 1.0f : 0.0f;
                        else
                            res.floatVal = val.floatVal;
                        break;

                    case Value::CHAR:
                        if (val.type == Value::INT)
                            res.charVal = (char)(val.intVal & 0xFF);
                        else
                            res.charVal = val.charVal;
                        res.intVal = (unsigned char)res.charVal;
                        break;

                    case Value::BOOL:
                        if (val.type == Value::FLOAT)
                            res.boolVal = (val.floatVal != 0.0f);
                        else
                            res.boolVal = (val.intVal != 0);
                        break;

                    case Value::STRING:
                        if (val.type == Value::INT)
                            res.strVal = std::to_string(val.intVal);
                        else if (val.type == Value::FLOAT)
                            res.strVal = std::to_string(val.floatVal);
                        else if (val.type == Value::CHAR)
                            res.strVal = std::string(1, val.charVal);
                        else if (val.type == Value::BOOL)
                            res.strVal = val.boolVal ? "true" : "false";
                        else
                            res.strVal = val.strVal;
                        break;

                    default:
                        break;
                    }

                    fr.stack.push_back(res);
                    break;
                }

                case OP_AND:
                case OP_OR:
                {
                    Value b = fr.stack.back();
                    fr.stack.pop_back();
                    Value a = fr.stack.back();
                    fr.stack.pop_back();
                    auto toBool = [](const Value &v) -> bool
                    {
                        if (v.type == Value::BOOL)
                            return v.boolVal;
                        if (v.type == Value::INT || v.type == Value::CHAR)
                            return v.intVal != 0;
                        if (v.type == Value::FLOAT)
                            return v.floatVal != 0.0f;
                        return false;
                    };
                    bool av = toBool(a);
                    bool bv = toBool(b);
                    Value res;
                    res.type = Value::BOOL;
                    res.boolVal = (op.op == OP_AND) ? (av && bv) : (av || bv);
                    fr.stack.push_back(res);
                    break;
                }

                case OP_PUSH_VD:
                {
                    Value v{};
                    v.type = Value::VD;
                    fr.stack.push_back(std::move(v));
                    break;
                }

                case OP_PUSH_DEFAULT:
                {
                    Value::Type ty = static_cast<Value::Type>(op.intArg);
                    Value v = defaultValue(ty);
                    fr.stack.push_back(std::move(v));
                    break;
                }

                default:
                    break;
                }
            }

            return Value{};
        }
    };
}

#endif
