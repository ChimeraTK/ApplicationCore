# CR-002: Remove DummyRegisterRawAccessor from the test fixture

Synopsis: Replace `ChimeraTK::DummyRegisterRawAccessor` in the shared test
fixture by the non-raw `ChimeraTK::DummyRegisterAccessor`. This removes the
raw dummy backdoor and its now mandatory explicit template argument.

Status: PLANNED

## Requirements

- `tests/include/fixtures.h` uses no `ChimeraTK::DummyRegisterRawAccessor`
  and no `DeviceBackend::getRawAccessor()`.
- The four `FixtureWithPollAndPushInput` members
  `exceptionDummyRegister`, `exceptionDummyRegister2`,
  `exceptionDummyRegister3` and `exceptionDummy2Register` become
  `ChimeraTK::DummyRegisterAccessor<int>`.
- Registers stay the same: device 1 `REG1`, `REG2`, `REG3`, device 2 `REG1`.
- Register access stays a direct memory access, unaffected by the simulated
  exceptions (`throwExceptionOpen`, `throwExceptionRead`,
  `throwExceptionWrite`) and by the read-only flag of `REG1`.
- Values, map files and test behaviour stay unchanged.

## Specifications

- Affected: `tests/include/fixtures.h`,
  `tests/executables_src/testExceptionHandling.cc`.
- The members carry a short comment stating their purpose: preset and verify
  device registers independently of the simulated device exceptions.
- Fixture members are constructed from `deviceBackend.get()` and
  `deviceBackend2.get()` with module `""` and the register name.
- Register names are `REG1`, `REG2`, `REG3`; the `DUMMY_WRITEABLE` alias is
  not needed, as the non-raw accessor ignores the read-only flag and writes
  the register memory directly.
- The deprecated `getBufferLock()` calls are dropped; the non-raw accessor
  locks internally.
- The unused `read<T>` helpers (both overloads) are removed.
- The `write` helper keeps its name and call sites, taking
  `ChimeraTK::DummyRegisterAccessor<int>&` and `int`.
- The two `getBufferLock()` checks in `testExceptionHandling.cc` (B_3_2_3)
  lose the lock.
- The fixture includes stay unchanged; `ChimeraTK/DummyRegisterAccessor.h`
  declares the non-raw accessor as well.
- All four registers are 32 bit with 0 fractional bits, so the fixed-point
  conversion of the non-raw accessor is the identity and the used test
  values stay valid.
- The `getBufferLock()` calls in `testInitialValueSpecD8.cc` are out of
  scope.

### Alternatives considered

- Front-door accessors for the write members (with `REG1.DUMMY_WRITEABLE`
  suffix) and non-raw accessors for the read members: the read members
  cannot use the front door, because B_3_2_3 checks the register content
  while `throwExceptionRead` is set, where a front-door read throws.
  Mixing both flavours in one fixture gives no benefit.
- Explicit template argument for the raw accessor
  (`DummyRegisterRawAccessor<int32_t>`): keeps the raw backdoor and its
  skipped conversion, which the fixture does not need.

## Test plan

- No new tests; the change is inside the test fixture.
- `testExceptionHandling`, the only user of the four members, builds and
  passes unchanged.
- `testVersionPropagation`, which compiles the fixture, builds unchanged.
