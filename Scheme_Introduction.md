# Scheme Introduction

This repository implements a first-order byte-wise masked AES, using Thumb assembly with GNU syntax. The underlying SCA protection scheme is indeed the exemplary one in the DPA book\(Chapter 9, page 228\)\[MOP07\], although some additional tricks have been applied to enhance its security on this specific M0 core (NXP LPC1114).

## Scheme description

### Basic AES
The underlying AES implementation here is rather trivial, nonetheless, here we list a few (un-)useful bullet points:
- Sbox implemented as a 256B table
- Byte-wise implementation, no T-table
- All round keys are pre-computed in memory, rather than computed online
- Key expansion should be called before encryption, note that this part is not masked
- One unusual feature of this implementation is, instead of AES's column-based ordering, here we use the row-based byte order. This means the state array is stored in memory as "State[0][0], State[0][1], State[0][2], State[0][3]" rather than the well-known AES order "State[0][0], State[1][0], State[2][0], State[3][0]". Such implementation allows us to perform the "ShiftRow" with only a few "ROR" instructions in Thumb assembly, rather than copying each of the 16 bytes separately. 

### Security Patches
If you have not read the original scheme on the DPA book, the following discussion might be confusing: you can always go back to the original scheme or try our "mask flow" section directly.

Here we list a few possible implementation threats not mentioned by the original masking scheme.  Whether they actually happen in practice depends on the specific platform. Nonetheless, it might be worth keeping an eye on such issues:
- Memory Effect 
-- As we probably already know, both the read-bus and write-bus (or any associated buffers) will be charged with loading/storing data. In the original scheme, all 16 bytes of the masked state use the same mask U. Any transition between such data will be revealing unmasked data, which leads to exploitable 1st order leakage.
-- Even if the read/write buses have been effectively cleared, memory buses are more likely to be 32-bit rather than 8-bit. This means even if we are using an "LDRB/STRB" instruction (load/store byte in Thumb assembly), there is no guarantee that the leakage will be solely about the byte we choose to load. All other 3 discarding bytes may also appear somewhere, contributing to the leakage we have in the end. in the masked Sbox, as all 256 bytes share the same data mask V, loading a 32-bit word means 4 bytes protected by the same mask might appear on the bus. If there is any interaction between these 4 bytes, 1st order leakage will appear. Although the amplitude will not be as significant as most HW/HD leakages, they can possibly save the attackers from searching through higher order moments.
-- In general, all registers/buffers should be cleared beforehand, before storing any new data. This is usually ensured by writing some constant value to a certain address or register in assembly.

- Rotations
-- For efficiency, this implementation uses the row-based ordering, aiming at performing ShiftRow with a few word-wise rotation instructions. However, from a security perspective, this is indeed a tricky task: we have seen that certain rotations may combine leakages between different bits, which leads to first order leakage if any two bits share the same mask within this 32-bit word. Obviously, following the original masking scheme, the output of Sbox will be 16 bytes of state, sharing the same mask U. That means if you are rotating a 32-bit word, you will have 1st order leakage.

-- Bear in mind that we cannot see all the micro-architectural level buses. Barrel shifter can have its own bus, which might not be cleared until next shift instruction is executed. For some CPUs, it may make sense to manually write a rotation for some constant, making sure that anything related to the last rotation has already been cleared out.

- MixColumn Specific
-- Unlike ShiftRow, MixColumn uses much more complicated operations. Although the original version of this code tried to protect MixColumn with only two masks (U and V), the security evaluation suggests this is an impossibly difficult task. Here we confirm the observation in the DPA book \[MOP07\], that protecting MixColumn can be a lot easier with 4 bytes additional masks.


### Mask Flow
The scheme utilizes 6 random bytes for each encryption. In the SCALE version, we ask for 6 bytes fresh random bytes from the serial port, which eliminates the need for generating randomness on board. These 6 bytes are used as:

* U and V: Byte masks for the state and round key. The state array should be protected by U and the round key by V. 

* 4-byte state word mask (W): in our code denoted as SRMask, as it is originally aiming at protected the rotation in ShiftRow. Later on, this 32-bit word mask is actually kept throughout the whole encryption process.

* 4-byte Sbox word mask (SW): a temporal mask for protecting the byte-wise masked Sbox from 1st order leakage. We took an adventurous step here, saving the randomness by using the same mask as the state word mask (W). Note that this might leave room for the attacker to exploit the collisions of those masks.

* 4-byte MixColumn word mask (MW): a temporal mask for protecting the MixColumn from 1st order leakage. In the original version, David took full advantage of U and V to avoid distance-based leakages. However, as we have discussed before, any loading/storing/rotating may create 1st order leakage within one 32-bit word, suppose two bytes share the same mask within that word. To this end, we decide to use a full word-size temporal mask for MixColumn. To save randomness, here we also took an adventurous step, using the reverse ordered W as MW. In other words, MW[0]=W[3], MW[1]=W[2], MW[2]=W[1], MW[3]=W[0]. Note that this might leave room for the attacker to exploit the collisions of those masks.


The full encryption procedure is as follow:
- GenMaskedSbox: this function generates the masked Sbox table , as S''(x)=S(x^U^V)^U^SW[x&0x3] (SW=SRMask in our code). Sbox mask SW makes sure that no two bytes within one word have the same mask.

- MaskingPlaintext: this function masks the unshared data m[i] with mask U^W[i&0x3](W=SRMask in our code). The purpose of W is also preventing 1st order leakage within one word [Mask transfer flow: (0,0,0,0)->(U^W[0],U^W[1],U^W[2],U^W[3])].

- MaskingKey: this function changes the unshared round key to the row-based ordering, then masks with V [Mask transfer flow: (0,0,0,0)->(V, V, V, V)].

- MADK: a word-wise version of the AddRoundKey, nothing special except here we are doing it word-wise instead of byte-wise [Mask transfer flow: (U^W[0],U^W[1],U^W[2],U^W[3])^(V,V,V,V)->(U^V^W[0],U^V^W[1],U^V^W[2],U^V^W[3])]. 

- MSbox: this function serves as the masked Sbox computation. It starts with unmasking the input with the state word-wise mask W, then looks up the masked table S''. The Sbox word-wise mask SW will be removed first, then added back the mask Inv_SR(W), where Inv_SR is the inverse transformation of ShiftRow. As we have explained before, the purpose of adding back state word-wise mask W is preventing 1st order leakage in storing/loading/rotating. Inverse ShiftRow is solely for efficiency: doing this here means after ShiftRow, we will have the state word-wise mask W back, rather than having a ShiftRow version of W then removing it afterward. [Mask transfer flow (first row): (U^V^W[0],U^V^W[1],U^V^W[2],U^V^W[3])->(U^V,U^V,U^V,U^V)->(U^SW[*], U^SW[*], U^SW[*], U^SW[*])->(U, U, U, U)-> (U^W[0],U^W[1],U^W[2],U^W[3])]. 

- MShiftRow: The main working part of ShiftRow is just 3 rotation instructions (the first row stays the same in AES). Considering it may use the memory bus, there are other instructions to clear the data bus or the barrel shifter.[Mask transfer flow (first row): (U^W[0],U^W[1],U^W[2],U^W[3]) ->(U^W[0],U^W[1],U^W[2],U^W[3])].
 
- MMixColumn: The main issue of masked MixColumn is the combining bytes always come from the same position within a word, so that they share both mask U and W. This implementation computes AES's (2,3,1,1) as Y=2*(X[0]^X[1])^X[1]^X[2]^X[3]. Clearly, the mask will fall off for X[0]^X[1]. The workaround here is we use another word mask MW, compute Y=2*(X[0]^MW^X[1])^X[1]^X[2]^X[3]^2*MW. MW will be canceled out in the end, whereas U and W will stick to the masked state. Note that MW cannot be the same as W, otherwise X[0]^MW^X[1] will always leak something, in whatever order. To save randomness, here we use the reverse ordered W as MW.[Mask transfer flow (first row): (U^W[0],U^W[1],U^W[2],U^W[3]) ->(U^W[0],U^W[1],U^W[2],U^W[3])].

- SafeCopy: this function copies 4 words from one buffer to another buffer. The target buffer/read bus/write bus will be carefully cleared each time. Will not affect any mask. 

- Finalize: changes the row-based ordering back to column-based ordering, removes the state mask.[Mask tranfer flow: (U^W[0],U^W[1],U^W[2],U^W[3])->(0,0,0,0)].

## References
\[MOP07\] Mangard S, Oswald E, Popp T. Power analysis attacks: Revealing the secrets of smart cards. Springer Science &amp;Business Media, 2007.