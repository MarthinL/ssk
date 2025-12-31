# Implementation Concerns

## Context Diagram

As a repository and C Language project, SSK is misleadingly lobsided. The core concept is incredibly simple once grasped, with full implementation of the trivial case probably not even worth adding a second source file. Meanwhile the code base is dominated by the complexities arising from a secondary problem - scaling that super-simple core to a the required domain.  



This model seeks to create stable, useful separation of concerns to manage that complexity better by drawing clear lines with well defined interactions between them. 



Do not expect to see a source file per concern, that is not the way.

![A0: The SSK Extension](./diagrams/dgm824.svg)

### Area A0: The SSK Extension
|  | Role        | Name                                   | Details                               |
|--| :---------  | :------------------------------------- | ------------------------------------- |
|  | Impetus     | Call Parameter(s)                      |                                       |
|  | Impetus     | Extension Function Call                |                                       |
|  |             |                                        |                                       |
|  | Constraint  | Encoding Specification (Format 0, CDU) |                                       |
|  | Constraint  | Formal SSK Type Definition             |                                       |
|  |             |                                        |                                       |
|  | Outcome     | Call Return Value                      |                                       |
|  |             |                                        |                                       |
|  | Means       | PostgreSQL Server                      |                                       |

---

## Separation of Concerns in Area A0: The SSK Extension

THIS IS AN EXAMPLE

![A0: The SSK Extension](./diagrams/dgm1305.svg)

### Interaction
| Role        | *#  | Name                                   |
| :---------  |:---:| :------------------------------------- |
| Impetus     | I1  | Call Parameter(s)                      |
| Impetus     | I2  | Extension Function Call                |
|             |     |                                        |
| Constraint  | C1  | Encoding Specification (Format 0, CDU) |
| Constraint  | C2  | Formal SSK Type Definition             |
|             |     |                                        |
| Outcome     | O1  | Call Return Value                      |
|             |     |                                        |
| Means       | M1  | PostgreSQL Server                      |

### Constituent Areas

#### A1: Value Decoder
|  | Role       | *# | Name                                   | Details                               |
|--| :--------- |:--:| -------------------------------------- | ------------------------------------- |
|  | Impetus    |    | Given Value                            | Type of I1: Call Parameter(s)         |
|  |            |    |                                        |                                       |
|  | Constraint | C1 | Encoding Specification (Format 0, CDU) |                                       |
|  |            |    |                                        |                                       |
|  | Outcome    |    | Given as AbV                           | Impetus of A2: Function Processor     |
|  |            |    |                                        |                                       |
|  | Means      | M1 | PostgreSQL Server                      |                                       |

#### A2: Function Processor
|  | Role       | *# | Name                                   | Details                               |
|--| :--------- |:--:| -------------------------------------- | ------------------------------------- |
|  | Impetus    |    | Given as AbV                           | Outcome of A1: Value Decoder          |
|  | Impetus    |    | Given BIGINT                           | Type of I1: Call Parameter(s)         |
|  | Impetus    |    | Given BIGINT[]                         | Type of I1: Call Parameter(s)         |
|  |            |    |                                        |                                       |
|  | Constraint | C2 | Formal SSK Type Definition             |                                       |
|  | Constraint | I2 | Extension Function Call                |                                       |
|  |            |    |                                        |                                       |
|  | Outcome    |    | Resulting BIGINT[]                     | Type of O1: Call Return Value         |
|  | Outcome    |    | Resulting BOOLEAN                      | Type of O1: Call Return Value         |
|  | Outcome    |    | Resulting BIGINT                       | Type of O1: Call Return Value         |
|  | Outcome    |    | Canonical Result AbV                   | Impetus of A3: Value Encoder          |
|  |            |    |                                        |                                       |
|  | Means      | M1 | PostgreSQL Server                      |                                       |

#### A3: Value Encoder
|  | Role       | *# | Name                                   | Details                               |
|--| :--------- |:--:| -------------------------------------- | ------------------------------------- |
|  | Impetus    |    | Canonical Result AbV                   | Outcome of A2: Function Processor     |
|  |            |    |                                        |                                       |
|  | Constraint | C1 | Encoding Specification (Format 0, CDU) |                                       |
|  |            |    |                                        |                                       |
|  | Outcome    |    | Resulting Value                        | Type of O1: Call Return Value         |
|  |            |    |                                        |                                       |
|  | Means      | M1 | PostgreSQL Server                      |                                       |

---

## Separation of Concerns in Area A1: Value Decoder

![A1: Value Decoder](./diagrams/dgm1320.svg)

### Interaction
| Role        | *#  | Name                                   |
| :---------  |:---:| :------------------------------------- |
| Impetus     | I1  | Given Value                            |
|             |     |                                        |
| Constraint  | C1  | Encoding Specification (Format 0, CDU) |
|             |     |                                        |
| Outcome     | O1  | Given as AbV                           |
|             |     |                                        |
| Means       | M1  | PostgreSQL Server                      |

### Constituent Areas

#### A11: Use existing AbV
|  | Role       | *# | Name                                   | Details                               |
|--| :--------- |:--:| -------------------------------------- | ------------------------------------- |
|  | Impetus    | I1 | Given Value                            |                                       |
|  |            |    |                                        |                                       |
|  | Outcome    | O1 | Given as AbV                           |                                       |
|  | Outcome    |    | Value Bytes                            | Impetus of A12: Decode AbV            |
|  |            |    |                                        |                                       |
|  | Means      | M1 | PostgreSQL Server                      |                                       |

#### A12: Decode AbV
|  | Role       | *# | Name                                   | Details                               |
|--| :--------- |:--:| -------------------------------------- | ------------------------------------- |
|  | Impetus    |    | Value Bytes                            | Outcome of A11: Use existing AbV      |
|  | Impetus    |    | Token Value                            | Outcome of A13: Decode Token          |
|  |            |    |                                        |                                       |
|  | Constraint | C1 | Encoding Specification (Format 0, CDU) |                                       |
|  |            |    |                                        |                                       |
|  | Outcome    |    | Decoded AbV                            | Impetus of A14: Remember AbV          |
|  | Outcome    |    | Token Type                             |                                       |
|  | Outcome    |    | Value Buffer                           | Impetus of A13: Decode Token          |
|  |            |    |                                        |                                       |
|  | Means      | M1 | PostgreSQL Server                      |                                       |

#### A13: Decode Token
|  | Role       | *# | Name                                   | Details                               |
|--| :--------- |:--:| -------------------------------------- | ------------------------------------- |
|  | Impetus    |    | Value Buffer                           | Outcome of A12: Decode AbV            |
|  |            |    |                                        |                                       |
|  | Constraint |    | Token Type                             |                                       |
|  | Constraint |    | CDU Types                              | Part of C1: Encoding Specification (Format 0, CDU) |
|  |            |    |                                        |                                       |
|  | Outcome    |    | Token Value                            | Impetus of A12: Decode AbV            |
|  |            |    |                                        |                                       |
|  | Means      | M1 | PostgreSQL Server                      |                                       |

#### A14: Remember AbV
|  | Role       | *# | Name                                   | Details                               |
|--| :--------- |:--:| -------------------------------------- | ------------------------------------- |
|  | Impetus    |    | Decoded AbV                            | Outcome of A12: Decode AbV            |
|  |            |    |                                        |                                       |
|  | Outcome    | O1 | Given as AbV                           |                                       |

---

## Separation of Concerns in Area A2: Function Processor

![A2: Function Processor](./diagrams/dgm1382.svg)

### Interaction
| Role        | *#  | Name                                   |
| :---------  |:---:| :------------------------------------- |
| Impetus     | I1  | Given as AbV                           |
| Impetus     | I2  | Given BIGINT                           |
| Impetus     | I3  | Given BIGINT[]                         |
|             |     |                                        |
| Constraint  | C1  | Formal SSK Type Definition             |
| Constraint  | C2  | Extension Function Call                |
|             |     |                                        |
| Outcome     | O1  | Resulting BIGINT[]                     |
| Outcome     | O2  | Resulting BOOLEAN                      |
| Outcome     | O3  | Resulting BIGINT                       |
| Outcome     | O4  | Canonical Result AbV                   |
|             |     |                                        |
| Means       | M1  | PostgreSQL Server                      |

### Constituent Areas

#### A21: SSK/AGG Function Exec
|  | Role       | *# | Name                                   | Details                               |
|--| :--------- |:--:| -------------------------------------- | ------------------------------------- |
|  | Impetus    | I1 | Given as AbV                           |                                       |
|  | Impetus    | I2 | Given BIGINT                           |                                       |
|  | Impetus    | I3 | Given BIGINT[]                         |                                       |
|  |            |    |                                        |                                       |
|  | Constraint | C1 | Formal SSK Type Definition             |                                       |
|  | Constraint | C2 | Extension Function Call                |                                       |
|  |            |    |                                        |                                       |
|  | Outcome    | O3 | Resulting BIGINT                       |                                       |
|  | Outcome    | O2 | Resulting BOOLEAN                      |                                       |
|  | Outcome    | O1 | Resulting BIGINT[]                     |                                       |
|  | Outcome    |    | AbV Param2                             | Impetus of A23: Fragment Input(s)     |
|  | Outcome    |    | BIGINT[] Param                         | Impetus of A23: Fragment Input(s)     |
|  | Outcome    |    | AbV Param                              | Impetus of A22: Determine output by AvB |
|  | Outcome    |    | Function Callback                      |                                       |
|  | Outcome    |    | BIGINT Param                           | Impetus of A22: Determine output by AvB |
|  |            |    |                                        |                                       |
|  | Means      | M1 | PostgreSQL Server                      |                                       |

#### A22: Determine output by AvB
|  | Role       | *# | Name                                   | Details                               |
|--| :--------- |:--:| -------------------------------------- | ------------------------------------- |
|  | Impetus    |    | BIGINT Param                           | Outcome of A21: SSK/AGG Function Exec |
|  | Impetus    |    | AbV Param                              | Outcome of A21: SSK/AGG Function Exec |
|  |            |    |                                        |                                       |
|  | Constraint |    | Function Callback                      |                                       |
|  |            |    |                                        |                                       |
|  | Outcome    | O3 | Resulting BIGINT                       |                                       |
|  | Outcome    | O2 | Resulting BOOLEAN                      |                                       |
|  | Outcome    | O1 | Resulting BIGINT[]                     |                                       |
|  | Outcome    |    | Dirty AvB                              | Impetus of A25: Normalise (Clean & Defrag) |
|  |            |    |                                        |                                       |
|  | Means      | M1 | PostgreSQL Server                      |                                       |

#### A23: Fragment Input(s)
|  | Role       | *# | Name                                   | Details                               |
|--| :--------- |:--:| -------------------------------------- | ------------------------------------- |
|  | Impetus    |    | AbV Param2                             | Outcome of A21: SSK/AGG Function Exec |
|  | Impetus    |    | BIGINT[] Param                         | Outcome of A21: SSK/AGG Function Exec |
|  | Impetus    |    | AbV Param                              | Outcome of A21: SSK/AGG Function Exec |
|  |            |    |                                        |                                       |
|  | Constraint |    | Function Callback                      |                                       |
|  |            |    |                                        |                                       |
|  | Outcome    |    | Fresh FragmentSet                      | Impetus of A24: Determine Output per Fragment |
|  |            |    |                                        |                                       |
|  | Means      | M1 | PostgreSQL Server                      |                                       |

#### A24: Determine Output per Fragment
|  | Role       | *# | Name                                   | Details                               |
|--| :--------- |:--:| -------------------------------------- | ------------------------------------- |
|  | Impetus    |    | Fresh FragmentSet                      | Outcome of A23: Fragment Input(s)     |
|  |            |    |                                        |                                       |
|  | Constraint |    | Function Callback                      |                                       |
|  |            |    |                                        |                                       |
|  | Outcome    | O3 | Resulting BIGINT                       |                                       |
|  | Outcome    | O2 | Resulting BOOLEAN                      |                                       |
|  | Outcome    | O1 | Resulting BIGINT[]                     |                                       |
|  | Outcome    |    | Dirty FragmentSet                      | Impetus of A25: Normalise (Clean & Defrag) |
|  |            |    |                                        |                                       |
|  | Means      | M1 | PostgreSQL Server                      |                                       |

#### A25: Normalise (Clean & Defrag)
|  | Role       | *# | Name                                   | Details                               |
|--| :--------- |:--:| -------------------------------------- | ------------------------------------- |
|  | Impetus    |    | Dirty AvB                              | Outcome of A22: Determine output by AvB |
|  | Impetus    |    | Dirty FragmentSet                      | Outcome of A24: Determine Output per Fragment |
|  |            |    |                                        |                                       |
|  | Outcome    | O4 | Canonical Result AbV                   |                                       |
|  |            |    |                                        |                                       |
|  | Means      | M1 | PostgreSQL Server                      |                                       |

---

## Separation of Concerns in Area A3: Value Encoder

![A3: Value Encoder](./diagrams/dgm1450.svg)

### Interaction
| Role        | *#  | Name                                   |
| :---------  |:---:| :------------------------------------- |
| Impetus     | I1  | Canonical Result AbV                   |
|             |     |                                        |
| Constraint  | C1  | Encoding Specification (Format 0, CDU) |
|             |     |                                        |
| Outcome     | O1  | Resulting Value                        |
|             |     |                                        |
| Means       | M1  | PostgreSQL Server                      |

### Constituent Areas

#### A31: Encode AbV
|  | Role       | *# | Name                                   | Details                               |
|--| :--------- |:--:| -------------------------------------- | ------------------------------------- |
|  | Impetus    | I1 | Canonical Result AbV                   |                                       |
|  | Impetus    |    | Encoded Token                          | Outcome of A32: Encode Token          |
|  |            |    |                                        |                                       |
|  | Constraint | C1 | Encoding Specification (Format 0, CDU) |                                       |
|  |            |    |                                        |                                       |
|  | Outcome    |    | Encoded Value                          | Impetus of A33: Conditionally Remember AbV |
|  | Outcome    |    | Token to Encode                        | Impetus of A32: Encode Token          |
|  |            |    |                                        |                                       |
|  | Means      | M1 | PostgreSQL Server                      |                                       |

#### A32: Encode Token
|  | Role       | *# | Name                                   | Details                               |
|--| :--------- |:--:| -------------------------------------- | ------------------------------------- |
|  | Impetus    |    | Token to Encode                        | Outcome of A31: Encode AbV            |
|  |            |    |                                        |                                       |
|  | Constraint |    | CDU Types                              | Part of C1: Encoding Specification (Format 0, CDU) |
|  |            |    |                                        |                                       |
|  | Outcome    |    | Encoded Token                          | Impetus of A31: Encode AbV            |
|  |            |    |                                        |                                       |
|  | Means      | M1 | PostgreSQL Server                      |                                       |

#### A33: Conditionally Remember AbV
|  | Role       | *# | Name                                   | Details                               |
|--| :--------- |:--:| -------------------------------------- | ------------------------------------- |
|  | Impetus    |    | Encoded Value                          | Outcome of A31: Encode AbV            |
|  |            |    |                                        |                                       |
|  | Outcome    | O1 | Resulting Value                        |                                       |
|  |            |    |                                        |                                       |
|  | Means      | M1 | PostgreSQL Server                      |                                       |

---
