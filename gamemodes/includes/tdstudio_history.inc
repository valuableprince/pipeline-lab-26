#if defined _tarkenso_tds_history
    #endinput
#endif
#define _tarkenso_tds_history

#define TDS_HISTORY_DEPTH (20)
#define TDS_H_STATES (TDS_HISTORY_DEPTH+1)
#define TDS_H_PACK(%0) (((%0)+3)/4)
#define TDS_H_CELLS (1+TDS_H_PACK(TDS_NAME)+TDS_H_PACK(TDS_DESCRIPTION)+TDS_ELEMENTS*(sizeof(gTDSFields)+1+TDS_H_PACK(TDS_TEXT))+TDS_GROUPS*(1+TDS_H_PACK(TDS_NAME)))
#define TDS_H_CREATE (1)
#define TDS_H_DELETE (2)
#define TDS_H_COPY (3)
#define TDS_H_MOVE (4)
#define TDS_H_RESIZE (5)
#define TDS_H_SCALE (6)
#define TDS_H_GEOMETRY (7)
#define TDS_H_LAYER (8)
#define TDS_H_GROUP (9)
#define TDS_H_META (10)
#define TDS_H_INSERT (11)
#define TDS_H_ALIGN (12)
#define TDS_H_DISTRIBUTE (13)
#define TDS_H_SCREEN (14)
#define TDS_H_GROUP_CREATE (15)
#define TDS_H_GROUP_RENAME (16)
#define TDS_H_GROUP_ASSIGN (17)
#define TDS_H_GROUP_DISSOLVE (18)
#define TDS_H_PROP (100)

// Adjacent operations share endpoints. Strings use four CP1251 bytes per cell.
// Images contain logical IDs, never native handles/player IDs/SQL job IDs.
new gTDSHistory[TDS_SESSIONS][TDS_H_STATES][TDS_H_CELLS];
new gTDSHistoryLabel[TDS_SESSIONS][TDS_H_STATES][64];
new gTDSHistoryKind[TDS_SESSIONS][TDS_H_STATES];
new gTDSCheckpoint[TDS_SESSIONS][TDS_H_CELLS];
new bool:gTDSCheckpointValid[TDS_SESSIONS], bool:gTDSHistoryReady[TDS_SESSIONS];
new gTDSHistoryHead[TDS_SESSIONS], gTDSHistoryCount[TDS_SESSIONS], gTDSHistoryAt[TDS_SESSIONS];
new gTDSHistoryImage[TDS_H_CELLS];

stock TDS_HPack(const source[], image[], &at, cells)
{
    new length = strlen(source);
    for (new i; i < cells; i++)
    {
        new value;
        for (new b; b < 4; b++) if (i*4+b < length) value |= (source[i*4+b] & 255) << (b*8);
        image[at++] = value;
    }
    return 1;
}

stock TDS_HUnpack(const image[], &at, dest[], capacity)
{
    new cells = TDS_H_PACK(capacity);
    for (new i; i < capacity; i++) dest[i] = (image[at+i/4] >>> ((i%4)*8)) & 255;
    at += cells;
    // Require a terminator, rather than silently truncate a corrupt image.
    return dest[capacity-1] == EOS;
}

stock TDS_HCapture(s, image[TDS_H_CELLS])
{
    if (s < 0 || s >= TDS_SESSIONS) return 0;
    for (new i; i < TDS_H_CELLS; i++) image[i] = 0;
    new at, slots[TDS_ELEMENTS], count = TDS_StateOrder(gTDSElement[s], slots, true);
    image[at++] = gTDS[s][tsInterface];
    TDS_HPack(gTDS[s][tsName], image, at, TDS_H_PACK(TDS_NAME));
    TDS_HPack(gTDS[s][tsDescription], image, at, TDS_H_PACK(TDS_DESCRIPTION));
    for (new i; i < TDS_ELEMENTS; i++)
    {
        if (i >= count) { at += sizeof(gTDSFields)+1+TDS_H_PACK(TDS_TEXT); continue; }
        new e = slots[i];
        for (new f; f < sizeof(gTDSFields); f++) image[at++] = gTDSElement[s][e][gTDSFields[f]];
        image[at++] = gTDSElement[s][e][tdGroup];
        TDS_HPack(gTDSElement[s][e][tdText], image, at, TDS_H_PACK(TDS_TEXT));
    }
    new order[TDS_GROUPS], n;
    for (new g; g < TDS_GROUPS; g++) if (gTDSGroup[s][g][tgId])
    {
        new j = n;
        while (j && gTDSGroup[s][order[j-1]][tgId] > gTDSGroup[s][g][tgId]) { order[j] = order[j-1]; j--; }
        order[j] = g; n++;
    }
    for (new i; i < n; i++)
    {
        image[at++] = gTDSGroup[s][order[i]][tgId];
        TDS_HPack(gTDSGroup[s][order[i]][tgName], image, at, TDS_H_PACK(TDS_NAME));
    }
    return 1;
}

stock TDS_HSame(const a[TDS_H_CELLS], const b[TDS_H_CELLS])
{
    for (new i; i < TDS_H_CELLS; i++) if (a[i] != b[i]) return 0;
    return 1;
}

stock TDS_HWipeState(s, at)
{
    for (new f; f < TDS_H_CELLS; f++) gTDSHistory[s][at][f] = 0;
    for (new f; f < 64; f++) gTDSHistoryLabel[s][at][f] = 0;
    gTDSHistoryKind[s][at] = 0;
    return 1;
}

stock TDS_HReset(s)
{
    if (s < 0 || s >= TDS_SESSIONS) return 0;
    for (new i; i < TDS_H_STATES; i++) TDS_HWipeState(s, i);
    for (new f; f < TDS_H_CELLS; f++) gTDSCheckpoint[s][f] = 0;
    gTDSHistoryHead[s] = 0; gTDSHistoryAt[s] = 0; gTDSHistoryCount[s] = 0;
    gTDSCheckpointValid[s] = false; gTDSHistoryReady[s] = gTDS[s][tsUsed] && gTDS[s][tsProject];
    if (gTDSHistoryReady[s]) TDS_HCapture(s, gTDSHistory[s][0]);
    return 1;
}

stock TDS_HCount(s, bool:redo = false)
{
    if (s < 0 || s >= TDS_SESSIONS || !gTDS[s][tsUsed] || !gTDS[s][tsProject] || !gTDSHistoryReady[s]) return 0;
    if (gTDSHistoryHead[s] < 0 || gTDSHistoryHead[s] >= TDS_H_STATES || gTDSHistoryCount[s] < 0 ||
        gTDSHistoryCount[s] > TDS_HISTORY_DEPTH || gTDSHistoryAt[s] < 0 || gTDSHistoryAt[s] > gTDSHistoryCount[s]) return 0;
    return redo ? gTDSHistoryCount[s]-gTDSHistoryAt[s] : gTDSHistoryAt[s];
}

stock TDS_HDirty(s)
{
    if (s < 0 || s >= TDS_SESSIONS) return 0;
    if (!gTDSCheckpointValid[s]) return 1;
    TDS_HCapture(s, gTDSHistoryImage);
    gTDS[s][tsDirty] = !TDS_HSame(gTDSHistoryImage, gTDSCheckpoint[s]);
    return 1;
}

stock TDS_HCheckpoint(s)
{
    if (s < 0 || s >= TDS_SESSIONS || !gTDS[s][tsUsed] || !gTDS[s][tsProject]) return 0;
    TDS_HCapture(s, gTDSCheckpoint[s]); gTDSCheckpointValid[s] = true;
    gTDS[s][tsDirty] = false;
    return 1;
}

stock TDS_HRecord(s, kind, id = 0)
{
    if (s < 0 || s >= TDS_SESSIONS || !gTDS[s][tsUsed] || !gTDS[s][tsProject] || !TDS_ProjectValid(s)) return 0;
    if (!gTDSHistoryReady[s]) return TDS_HReset(s);
    TDS_HCapture(s, gTDSHistoryImage);
    new current = (gTDSHistoryHead[s]+gTDSHistoryAt[s])%TDS_H_STATES;
    if (TDS_HSame(gTDSHistoryImage, gTDSHistory[s][current])) { TDS_HDirty(s); return 0; }
    for (new i = gTDSHistoryAt[s]+1; i <= gTDSHistoryCount[s]; i++)
        TDS_HWipeState(s, (gTDSHistoryHead[s]+i)%TDS_H_STATES);
    gTDSHistoryCount[s] = gTDSHistoryAt[s]; // discard Redo only for a real new change
    if (gTDSHistoryAt[s] == TDS_HISTORY_DEPTH)
    {
        gTDSHistoryHead[s] = (gTDSHistoryHead[s]+1)%TDS_H_STATES;
        gTDSHistoryAt[s]--; gTDSHistoryCount[s]--;
    }
    new to = (gTDSHistoryHead[s]+gTDSHistoryAt[s]+1)%TDS_H_STATES;
    for (new f; f < TDS_H_CELLS; f++) gTDSHistory[s][to][f] = gTDSHistoryImage[f];
    new label[64], group = TDS_GroupSlot(s, gTDS[s][tsSelectedGroup]);
    switch (kind)
    {
        case TDS_H_CREATE: format(label, sizeof(label), "Создан TextDraw #%d", id);
        case TDS_H_DELETE: format(label, sizeof(label), "Удалён элемент #%d", id);
        case TDS_H_COPY: format(label, sizeof(label), "Дублирован элемент #%d", id);
        case TDS_H_MOVE:
        {
            if (group >= 0) format(label, sizeof(label), "Перемещена группа %s", gTDSGroup[s][group][tgName]);
            else format(label, sizeof(label), "Перемещено выделение / элемент #%d", id);
        }
        case TDS_H_RESIZE: format(label, sizeof(label), "Изменён размер элемента #%d", id);
        case TDS_H_SCALE: format(label, sizeof(label), "Масштабировано выделение / группа");
        case TDS_H_GEOMETRY: format(label, sizeof(label), "Изменена точная геометрия элемента #%d", id);
        case TDS_H_ALIGN: format(label, sizeof(label), "Выровнены элементы / группа");
        case TDS_H_DISTRIBUTE: format(label, sizeof(label), "Распределены выбранные элементы");
        case TDS_H_SCREEN: format(label, sizeof(label), "Выделение размещено относительно экрана");
        case TDS_H_LAYER: format(label, sizeof(label), "Изменён порядок слоёв");
        case TDS_H_GROUP: format(label, sizeof(label), "Изменена группа #%d / её состав", id);
        case TDS_H_GROUP_CREATE: format(label, sizeof(label), "Создана группа #%d", id);
        case TDS_H_GROUP_RENAME: format(label, sizeof(label), "Переименована группа #%d", id);
        case TDS_H_GROUP_ASSIGN: format(label, sizeof(label), "Изменён состав группы #%d", id);
        case TDS_H_GROUP_DISSOLVE: format(label, sizeof(label), "Расформирована группа #%d", id);
        case TDS_H_META: format(label, sizeof(label), "Изменены параметры проекта");
        case TDS_H_INSERT: format(label, sizeof(label), "Вставлен шаблон #%d", id);
        default:
        {
            if (kind == TDS_H_PROP+_:TDS_COLOUR || kind == TDS_H_PROP+_:TDS_ALPHA)
                format(label, sizeof(label), "Изменён цвет / alpha элемента #%d", id);
            else if (kind == TDS_H_PROP+_:TDS_CONTENT) format(label, sizeof(label), "Изменён текст / спрайт элемента #%d", id);
            else format(label, sizeof(label), "Изменены свойства элемента #%d", id);
        }
    }
    format(gTDSHistoryLabel[s][to], 64, "%s", label);
    gTDSHistoryKind[s][to] = kind;
    gTDSHistoryAt[s]++; gTDSHistoryCount[s] = gTDSHistoryAt[s];
    TDS_HDirty(s); TDS_RenderStatus(s);
    return 1;
}

stock TDS_HClear(playerid)
{
    new s = TDS_LayoutSession(playerid);
    if (s < 0) return 0;
    for (new i; i < TDS_H_STATES; i++) TDS_HWipeState(s, i);
    gTDSHistoryHead[s] = 0; gTDSHistoryAt[s] = 0; gTDSHistoryCount[s] = 0;
    TDS_HCapture(s, gTDSHistory[s][0]); TDS_RenderStatus(s);
    return 1; // checkpoint survives clearing the operation list
}

stock TDS_HStep(playerid, bool:redo = false)
{
    new s = TDS_LayoutSession(playerid);
    if (s < 0 || !TDS_HCount(s, redo)) return 0;
    new target = gTDSHistoryAt[s]+(redo ? 1 : -1), at;
    new operation = (gTDSHistoryHead[s]+(redo ? target : gTDSHistoryAt[s]))%TDS_H_STATES;
    new kind = gTDSHistoryKind[s][operation];
    if (!((kind >= TDS_H_CREATE && kind <= TDS_H_GROUP_DISSOLVE) ||
        (kind >= TDS_H_PROP && kind <= TDS_H_PROP+_:TDS_VEH_COLOURS))) return 0;
    new index = (gTDSHistoryHead[s]+target)%TDS_H_STATES;
    new interfaceType = gTDSHistory[s][index][at++], name[TDS_NAME], description[TDS_DESCRIPTION];
    if (!TDS_HUnpack(gTDSHistory[s][index], at, name, sizeof(name)) ||
        !TDS_HUnpack(gTDSHistory[s][index], at, description, sizeof(description))) return 0;
    for (new e; e < TDS_ELEMENTS; e++)
    {
        for (new E_TDS_ELEMENT:f; f < E_TDS_ELEMENT; f++) gTDSStage[e][f] = 0;
        for (new f; f < sizeof(gTDSFields); f++) gTDSStage[e][gTDSFields[f]] = gTDSHistory[s][index][at++];
        gTDSStage[e][tdGroup] = gTDSHistory[s][index][at++];
        if (!TDS_HUnpack(gTDSHistory[s][index], at, gTDSStage[e][tdText], TDS_TEXT)) return 0;
    }
    for (new g; g < TDS_GROUPS; g++)
    {
        gTDSStageGroups[g][tgId] = gTDSHistory[s][index][at++];
        if (!TDS_HUnpack(gTDSHistory[s][index], at, gTDSStageGroups[g][tgName], TDS_NAME)) return 0;
    }
    if (at != TDS_H_CELLS || !TDS_StateApply(s, name, description, interfaceType, gTDS[s][tsNextId], gTDS[s][tsNextGroupId]))
    { TDSHost_Message(playerid, ADMIN_ERROR, "История не применена: данные или ресурсы недоступны. Проект не изменён."); return 0; }
    gTDSHistoryAt[s] = target; gTDS[s][tsDirty] = true;
    TDS_HDirty(s); TDS_RenderStatus(s);
    return 1;
}
