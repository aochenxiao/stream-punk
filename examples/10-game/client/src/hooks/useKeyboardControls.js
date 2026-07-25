import { useState, useEffect, useCallback, useRef } from 'react'

const ANGLE_MIN = -90
const ANGLE_MAX = 90
const ANGLE_STEP = 5
const POWER_MIN = 10
const POWER_MAX = 100
const POWER_CHARGE_RATE = 80 // per second
const KEY_REPEAT_MS = 80
const MOVE_REPEAT_MS = 100

const WEAPON_NAMES = ['标准弹', '重型弹', '集束弹']

export function useKeyboardControls({ isMyTurn, currentWorm, onAim, onMove, onFire, onSwitchWeapon }) {
  const [angle, setAngle] = useState(45)
  const [power, setPower] = useState(0)
  const [charging, setCharging] = useState(false)
  const [weapon, setWeapon] = useState(0)

  const keysRef = useRef(new Set())
  const intervalsRef = useRef({})
  const chargingRef = useRef(false)
  const powerRef = useRef(0)
  const angleRef = useRef(angle)
  const weaponRef = useRef(0)
  const rafRef = useRef(null)
  const powerBarRef = useRef(null)  // DOM 引用，直接操作避免 React 批处理延迟

  // sync angle and weapon when turn starts
  useEffect(() => {
    if (isMyTurn && currentWorm) {
      setAngle(currentWorm.angle ?? 45)
      setPower(0)
      setCharging(false)
      chargingRef.current = false
      powerRef.current = 0
      const w = currentWorm.weapon ?? 0
      setWeapon(w)
      weaponRef.current = w
    }
  }, [isMyTurn, currentWorm])

  const sendAim = useCallback((newAngle) => {
    const clamped = Math.max(ANGLE_MIN, Math.min(ANGLE_MAX, newAngle))
    angleRef.current = clamped
    setAngle(clamped)
    onAim?.(clamped)
  }, [onAim])

  const startRepeat = useCallback((key, action, intervalMs) => {
    if (intervalsRef.current[key]) return
    action()
    intervalsRef.current[key] = setInterval(action, intervalMs)
  }, [])

  const stopRepeat = useCallback((key) => {
    const id = intervalsRef.current[key]
    if (id) {
      clearInterval(id)
      delete intervalsRef.current[key]
    }
  }, [])

  // 蓄力动画循环（直接操作 DOM 避免 React 批处理延迟）
  useEffect(() => {
    if (!charging) {
      if (rafRef.current) cancelAnimationFrame(rafRef.current)
      return
    }

    let lastTime = performance.now()
    let lastStateUpdate = 0
    const loop = (now) => {
      const dt = (now - lastTime) / 1000
      lastTime = now
      const nextPower = Math.min(POWER_MAX, powerRef.current + POWER_CHARGE_RATE * dt)
      powerRef.current = nextPower

      // 直接操作 DOM 进度条，丝滑 60fps
      if (powerBarRef.current) {
        powerBarRef.current.style.width = `${nextPower}%`
      }

      // 每 100ms 更新一次 React 状态（仅用于文字显示）
      if (now - lastStateUpdate > 100) {
        lastStateUpdate = now
        setPower(nextPower)
      }

      rafRef.current = requestAnimationFrame(loop)
    }
    rafRef.current = requestAnimationFrame(loop)

    return () => {
      if (rafRef.current) cancelAnimationFrame(rafRef.current)
    }
  }, [charging])

  useEffect(() => {
    // helper to read latest angle inside closures
    const angleRefValue = () => angleRef.current

    const handleKeyDown = (e) => {
      if (!isMyTurn) return
      if (keysRef.current.has(e.code)) return
      keysRef.current.add(e.code)

      if (e.code === 'KeyW') {
        e.preventDefault()
        startRepeat('KeyW', () => sendAim(angleRefValue() + ANGLE_STEP), KEY_REPEAT_MS)
      } else if (e.code === 'KeyS') {
        e.preventDefault()
        startRepeat('KeyS', () => sendAim(angleRefValue() - ANGLE_STEP), KEY_REPEAT_MS)
      } else if (e.code === 'KeyA') {
        e.preventDefault()
        startRepeat('KeyA', () => onMove?.(-1), MOVE_REPEAT_MS)
      } else if (e.code === 'KeyD') {
        e.preventDefault()
        startRepeat('KeyD', () => onMove?.(1), MOVE_REPEAT_MS)
      } else if (e.code === 'KeyQ') {
        e.preventDefault()
        const next = (weaponRef.current - 1 + 3) % 3
        weaponRef.current = next
        setWeapon(next)
        onSwitchWeapon?.(next)
      } else if (e.code === 'KeyE') {
        e.preventDefault()
        const next = (weaponRef.current + 1) % 3
        weaponRef.current = next
        setWeapon(next)
        onSwitchWeapon?.(next)
      } else if (e.code === 'Space') {
        e.preventDefault()
        if (!chargingRef.current) {
          chargingRef.current = true
          powerRef.current = 0
          setCharging(true)
        }
      }
    }

    const handleKeyUp = (e) => {
      keysRef.current.delete(e.code)

      if (e.code === 'KeyW') {
        stopRepeat('KeyW')
      } else if (e.code === 'KeyS') {
        stopRepeat('KeyS')
      } else if (e.code === 'KeyA') {
        stopRepeat('KeyA')
      } else if (e.code === 'KeyD') {
        stopRepeat('KeyD')
      } else if (e.code === 'Space') {
        if (chargingRef.current) {
          chargingRef.current = false
          setCharging(false)
          const finalPower = Math.max(POWER_MIN, powerRef.current)
          setPower(0)
          powerRef.current = 0
          onFire?.(finalPower)
        }
      }
    }

    window.addEventListener('keydown', handleKeyDown)
    window.addEventListener('keyup', handleKeyUp)

    return () => {
      window.removeEventListener('keydown', handleKeyDown)
      window.removeEventListener('keyup', handleKeyUp)
      Object.values(intervalsRef.current).forEach(clearInterval)
      intervalsRef.current = {}
      if (rafRef.current) cancelAnimationFrame(rafRef.current)
    }
  }, [isMyTurn, onAim, onMove, onFire, onSwitchWeapon, sendAim, startRepeat, stopRepeat])

  return { angle, power, charging, weapon, weaponName: WEAPON_NAMES[weapon] || '标准弹', powerBarRef }
}