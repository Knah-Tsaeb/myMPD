"use strict";
// SPDX-License-Identifier: GPL-3.0-or-later
// myMPD (c) 2018-2021 Juergen Mang <mail@jcgames.de>
// https://github.com/jcorporation/mympd

function initTimer() {
    document.getElementById('listTimerList').addEventListener('click', function(event) {
        event.stopPropagation();
        event.preventDefault();
        if (event.target.nodeName === 'TD') {
            if (!event.target.parentNode.classList.contains('not-clickable')) {
                showEditTimer(getCustomDomProperty(event.target.parentNode, 'data-id'));
            }
        }
        else if (event.target.nodeName === 'A') {
            deleteTimer(event.target, getCustomDomProperty(event.target.parentNode.parentNode, 'data-id'));
        }
        else if (event.target.nodeName === 'BUTTON') {
            toggleTimer(event.target, getCustomDomProperty(event.target.parentNode.parentNode, 'data-id'));
        }
    }, false);

    const selectTimerHourEl = document.getElementById('selectTimerHour');
    for (let i = 0; i < 24; i++) {
        selectTimerHourEl.appendChild(elCreateText('option', {"value": i}, zeroPad(i, 2)));
    }
    
    const selectTimerMinuteEl = document.getElementById('selectTimerMinute');
    for (let i = 0; i < 60; i = i + 5) {
        selectTimerMinuteEl.appendChild(elCreateText('option', {"value": i}, zeroPad(i, 2)));
    }

    document.getElementById('inputTimerVolume').addEventListener('change', function() {
        document.getElementById('textTimerVolume').textContent = this.value + ' %';
    }, false);
    
    document.getElementById('selectTimerAction').addEventListener('change', function() {
        selectTimerActionChange();
    }, false);

    document.getElementById('selectTimerInterval').addEventListener('change', function() {
        selectTimerIntervalChange();
    }, false);

    document.getElementById('modalTimer').addEventListener('shown.bs.modal', function () {
        showListTimer();
        hideModalAlert();
    });

    setCustomDomPropertyId('selectTimerPlaylist', 'data-cb-filter', 'filterPlaylistsSelect');
    setCustomDomPropertyId('selectTimerPlaylist', 'data-cb-filter-options', [0, 'selectTimerPlaylist']);
}

//eslint-disable-next-line no-unused-vars
function deleteTimer(el, timerid) {
    showConfirmInline(el.parentNode.previousSibling, tn('Do you really want to delete the timer?'), tn('Yes, delete it'), function() {
        sendAPI("MYMPD_API_TIMER_RM", {
            "timerid": timerid
        }, saveTimerCheckError, true);
    });
}

//eslint-disable-next-line no-unused-vars
function toggleTimer(target, timerid) {
    if (target.classList.contains('active')) {
        target.classList.remove('active');
        sendAPI("MYMPD_API_TIMER_TOGGLE", {
            "timerid": timerid,
            "enabled": false
        }, showListTimer);
    }
    else {
        target.classList.add('active');
        sendAPI("MYMPD_API_TIMER_TOGGLE", {
            "timerid": timerid,
            "enabled": true
        }, showListTimer);
    }
}

//eslint-disable-next-line no-unused-vars
function saveTimer() {
    let formOK = true;
    const nameEl = document.getElementById('inputTimerName');
    if (!validateNotBlank(nameEl)) {
        formOK = false;
    }
    let minOneDay = false;
    const weekdayBtns = ['btnTimerMon', 'btnTimerTue', 'btnTimerWed', 'btnTimerThu', 'btnTimerFri', 'btnTimerSat', 'btnTimerSun'];
    const weekdays = [];
    for (let i = 0, j = weekdayBtns.length; i < j; i++) {
        const checked = document.getElementById(weekdayBtns[i]).classList.contains('active') ? true : false;
        weekdays.push(checked);
        if (checked === true) {
            minOneDay = true;
        }
    }
    if (minOneDay === false) {
        formOK = false;
        elShowId('invalidTimerWeekdays');
    }
    else {
        elHideId('invalidTimerWeekdays');
    }
    const selectTimerAction  = document.getElementById('selectTimerAction');
    const jukeboxMode = getCustomDomProperty(document.getElementById('btnTimerJukeboxModeGroup').getElementsByClassName('active')[0], 'data-value');
    const selectTimerPlaylist = getCustomDomPropertyId('selectTimerPlaylist', 'data-value');

    if (selectTimerAction.selectedIndex === -1) {
        formOK = false;
        selectTimerAction.classList.add('is-invalid');
    }

    if (jukeboxMode === '0' &&  selectTimerPlaylist === 'Database'&& getSelectValue(selectTimerAction) === 'startplay') {
        formOK = false;
        document.getElementById('btnTimerJukeboxModeGroup').classList.add('is-invalid');
    }

    const inputTimerIntervalEl = document.getElementById('inputTimerInterval');
    if (!validateInt(inputTimerIntervalEl)) {
        formOK = false;
    }
    
    if (formOK === true) {
        const args = {};
        const argEls = document.getElementById('timerActionScriptArguments').getElementsByTagName('input');
        for (let i = 0, j = argEls.length; i < j; i++) {
            args[getCustomDomProperty(argEls[i], 'data-name')] = argEls[i].value;
        }
        let interval = Number(inputTimerIntervalEl.value);
        if (interval > 0) {
            interval = interval * 60 * 60;
        }
        sendAPI("MYMPD_API_TIMER_SAVE", {
            "timerid": Number(document.getElementById('inputTimerId').value),
            "name": nameEl.value,
            "interval": interval,
            "enabled": (document.getElementById('btnTimerEnabled').classList.contains('active') ? true : false),
            "startHour": Number(getSelectValueId('selectTimerHour')),
            "startMinute": Number(getSelectValueId('selectTimerMinute')),
            "weekdays": weekdays,
            "action": getCustomDomProperty(selectTimerAction.options[selectTimerAction.selectedIndex].parentNode, 'data-value'),
            "subaction": getSelectValue(selectTimerAction),
            "volume": Number(document.getElementById('inputTimerVolume').value), 
            "playlist": selectTimerPlaylist,
            "jukeboxMode": Number(jukeboxMode),
            "arguments": args
        }, saveTimerCheckError, true);
    }
}

function saveTimerCheckError(obj) {
    removeEnterPinFooter();
    if (obj.error) {
        showModalAlert(obj);
    }
    else {
        hideModalAlert();
        showListTimer();
    }
}

//eslint-disable-next-line no-unused-vars
function showEditTimer(timerid) {
    removeEnterPinFooter();
    elHideId('timerActionPlay');
    elHideId('timerActionScript');
    document.getElementById('listTimer').classList.remove('active');
    document.getElementById('editTimer').classList.add('active');
    elHideId('listTimerFooter');
    elShowId('editTimerFooter');
    document.getElementById('selectTimerPlaylist').filterInput.value = '';
        
    if (timerid !== 0) {
        sendAPI("MYMPD_API_TIMER_GET", {
            "timerid": timerid
        }, parseEditTimer);
    }
    else {
        filterPlaylistsSelect(1, 'selectTimerPlaylist', '', 'Database');
        document.getElementById('selectTimerPlaylist').value = tn('Database');
        setCustomDomPropertyId('selectTimerPlaylist', 'data-value', 'Database');

        document.getElementById('inputTimerId').value = '0';
        document.getElementById('inputTimerName').value = '';
        toggleBtnChkId('btnTimerEnabled', true);
        document.getElementById('selectTimerHour').value = '12';
        document.getElementById('selectTimerMinute').value = '0';
        document.getElementById('selectTimerAction').value = 'startplay';
        document.getElementById('inputTimerVolume').value = '50';
        document.getElementById('selectTimerPlaylist').value = 'Database';
        selectTimerIntervalChange(86400);
        selectTimerActionChange();
        toggleBtnGroupValue(document.getElementById('btnTimerJukeboxModeGroup'), 1);
        const weekdayBtns = ['btnTimerMon', 'btnTimerTue', 'btnTimerWed', 'btnTimerThu', 'btnTimerFri', 'btnTimerSat', 'btnTimerSun'];
        for (let i = 0, j = weekdayBtns.length; i < j; i++) {
            toggleBtnChkId(weekdayBtns[i], false);
        }
        elShowId('timerActionPlay');
    }
    document.getElementById('inputTimerName').focus();
    removeIsInvalid(document.getElementById('editTimerForm'));    
    document.getElementById('invalidTimerWeekdays').style.display = 'none';
}

function parseEditTimer(obj) {
    const playlistValue = obj.result.playlist;
    filterPlaylistsSelect(1, 'selectTimerPlaylist', '', playlistValue);
    document.getElementById('selectTimerPlaylist').value = playlistValue === 'Datbase' ? tn('Database'): playlistValue;
    setCustomDomPropertyId('selectTimerPlaylist', 'data-value', playlistValue);

    document.getElementById('inputTimerId').value = obj.result.timerid;
    document.getElementById('inputTimerName').value = obj.result.name;
    toggleBtnChkId('btnTimerEnabled', obj.result.enabled);
    document.getElementById('selectTimerHour').value = obj.result.startHour;
    document.getElementById('selectTimerMinute').value = obj.result.startMinute;
    document.getElementById('selectTimerAction').value = obj.result.subaction;
    selectTimerActionChange(obj.result.arguments);
    selectTimerIntervalChange(obj.result.interval);
    document.getElementById('inputTimerVolume').value = obj.result.volume;
    toggleBtnGroupValueId('btnTimerJukeboxModeGroup', obj.result.jukeboxMode);
    const weekdayBtns = ['btnTimerMon', 'btnTimerTue', 'btnTimerWed', 'btnTimerThu', 'btnTimerFri', 'btnTimerSat', 'btnTimerSun'];
    for (let i = 0, j = weekdayBtns.length; i < j; i++) {
        toggleBtnChkId(weekdayBtns[i], obj.result.weekdays[i]);
    }
}

function selectTimerIntervalChange(value) {
    if (value === undefined) {
        value = Number(getSelectValueId('selectTimerInterval'));
    }
    else {
        if (isNaN(value) || (value > 0 && value !== 86400 && value !== 604800)) {
            document.getElementById('selectTimerInterval').value = '';
        }
        else {
            document.getElementById('selectTimerInterval').value = value;
        }
    }
    if (isNaN(value) || (value > 0 && value !== 86400 && value !== 604800)) {
        elShowId('inputTimerInterval');
        elShowId('inputTimerIntervalLabel');
    }
    else {
        elHideId('inputTimerInterval');
        elHideId('inputTimerIntervalLabel');
    }
    document.getElementById('inputTimerInterval').value = isNaN(value) ? 1 : value > 0 ? (value / 60 / 60) : value;
}

function selectTimerActionChange(values) {
    const el = document.getElementById('selectTimerAction');
    
    if (getSelectValue(el) === 'startplay') {
        elShowId('timerActionPlay');
        elHideId('timerActionScript');
    }
    else if (el.selectedIndex > -1 && getCustomDomProperty(el.options[el.selectedIndex].parentNode, 'data-value') === 'script') {
        elShowId('timerActionScript');
        elHideId('timerActionPlay');
        showTimerScriptArgs(el.options[el.selectedIndex], values);
    }
    else {
        elHideId('timerActionPlay');
        elHideId('timerActionScript');
    }
}

function showTimerScriptArgs(option, values) {
    if (values === undefined) {
        values = {};
    }
    const args = JSON.parse(getCustomDomProperty(option, 'data-arguments'));
    const list = document.getElementById('timerActionScriptArguments');
    elClear(list);
    for (let i = 0, j = args.arguments.length; i < j; i++) {
        const input = elCreateEmpty('input', {"class": ["form-control"], "type": "text", "name": "timerActionScriptArguments" + i, 
            "value": (values[args.arguments[i]] ? values[args.arguments[i]] : '')});
        setCustomDomProperty(input, 'data-name', args.arguments[i]);
        const fg = elCreateNodes('div', {"class": ["form-group", "row"]},
            [
                elCreateText('label', {"class": ["col-sm-4", "col-form-label"], "for": "timerActionScriptArguments" + i}, args.arguments[i]),
                elCreateNode('div', {"class": ["col-sm-8"]}, input)
            ]
        );
        list.appendChild(fg);
    }
    if (args.arguments.length === 0) {
        list.textContent = tn('No arguments');
    }
}

function showListTimer() {
    removeEnterPinFooter();
    document.getElementById('listTimer').classList.add('active');
    document.getElementById('editTimer').classList.remove('active');
    elShowId('listTimerFooter');
    elHideId('editTimerFooter');
    sendAPI("MYMPD_API_TIMER_LIST", {}, parseListTimer, true);
}

function parseListTimer(obj) {
    const tbody = document.getElementById('listTimer').getElementsByTagName('tbody')[0];
    
    if (checkResult(obj, tbody, 5) === false) {
        return;
    }
    elClear(tbody);
    const weekdays = ['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun'];
    for (let i = 0; i < obj.result.returnedEntities; i++) {
        const row = document.createElement('tr');
        setCustomDomProperty(row, 'data-id', obj.result.data[i].timerid);
        row.appendChild(
            elCreateText('td', {}, obj.result.data[i].name)
        );
        const btn = elCreateEmpty('button', {"name": "enabled", "class": ["btn", "btn-secondary", "btn-xs", "mi", "mi-small"]});
        if (obj.result.data[i].enabled === true) {
            btn.classList.add('active');
            btn.textContent = 'check';
        }
        else {
            btn.textContent = 'radio_button_unchecked';
        }
        row.appendChild(elCreateNode('td', {}, btn));
        const days = [];
        for (let j = 0; j < 7; j++) {
            if (obj.result.data[i].weekdays[j] === true) {
                days.push(t(weekdays[j]));
            }
        }
        row.appendChild(
            elCreateText('td', {}, zeroPad(obj.result.data[i].startHour, 2) + ':' + zeroPad(obj.result.data[i].startMinute, 2) +
                ' ' + t('on') + ' ' + days.join(', '))
        );

        let interval = '';
        switch (obj.result.data[i].interval) {
            case 604800: interval = t('Weekly'); break;
            case 86400: interval = t('Daily'); break;
            case -1: interval = t('One shot and delete'); break;
            case 0: interval = t('One shot and disable'); break;
            default: interval = t('Each hours', obj.result.data[i].interval / 3600);
        }
        row.appendChild(
            elCreateText('td', {}, interval)
        );
        row.appendChild(
            elCreateText('td', {}, prettyTimerAction(obj.result.data[i].action, obj.result.data[i].subaction))
        );
        row.appendChild(
            elCreateNode('td', {"data-col": "Action"},
                elCreateText('a', {"class": ["mi", "color-darkgrey"], "href": "#"}, 'delete')
            )
        );
        tbody.append(row);
    }
}

function prettyTimerAction(action, subaction) {
    if (action === 'player' && subaction === 'startplay') {
        return t('Start playback');
    }
    if (action === 'player' && subaction === 'stopplay') {
        return t('Stop playback');
    }
    if (action === 'script') {
        return t('Script') + ': ' + e(subaction);
    }
    return e(action) + ': ' + e(subaction);
}
